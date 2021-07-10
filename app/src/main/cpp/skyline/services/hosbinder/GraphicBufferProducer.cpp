// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2005 The Android Open Source Project
// Copyright © 2019-2020 Ryujinx Team and Contributors

#include <gpu.h>
#include <gpu/texture/format.h>
#include <soc.h>
#include <services/nvdrv/driver.h>
#include <services/nvdrv/devices/nvmap.h>
#include <services/common/fence.h>
#include "GraphicBufferProducer.h"

namespace skyline::service::hosbinder {
    GraphicBufferProducer::GraphicBufferProducer(const DeviceState &state) : state(state), bufferEvent(std::make_shared<kernel::type::KEvent>(state, true)) {}

    u32 GraphicBufferProducer::GetPendingBufferCount() {
        u32 count{};
        for (auto it{queue.begin()}, end{it + activeSlotCount}; it < end; it++)
            if (it->state == BufferState::Queued)
                count++;
        return count;
    }

    AndroidStatus GraphicBufferProducer::RequestBuffer(i32 slot, GraphicBuffer *&buffer) {
        std::scoped_lock lock(mutex);
        if (slot < 0 || slot >= queue.size()) [[unlikely]] {
            state.logger->Warn("#{} was out of range", slot);
            return AndroidStatus::BadValue;
        }

        auto &bufferSlot{queue[slot]};
        bufferSlot.wasBufferRequested = true;
        buffer = bufferSlot.graphicBuffer.get();

        state.logger->Debug("#{}", slot);
        return AndroidStatus::Ok;
    }

    AndroidStatus GraphicBufferProducer::SetBufferCount(i32 count) {
        std::scoped_lock lock(mutex);
        if (count >= MaxSlotCount) [[unlikely]] {
            state.logger->Warn("Setting buffer count too high: {} (Max: {})", count, MaxSlotCount);
            return AndroidStatus::BadValue;
        }

        for (auto it{queue.begin()}; it != queue.end(); it++) {
            if (it->state == BufferState::Dequeued) {
                state.logger->Warn("Cannot set buffer count as #{} is dequeued", std::distance(queue.begin(), it));
                return AndroidStatus::BadValue;
            }
        }

        if (!count) {
            activeSlotCount = 0;
            bufferEvent->Signal();
            return AndroidStatus::Ok;
        }

        // We don't check minBufferSlots here since it's effectively hardcoded to 0 on HOS (See NativeWindowQuery::MinUndequeuedBuffers)

        // HOS only resets all the buffers if there's no preallocated buffers, it simply sets the active buffer count otherwise
        if (preallocatedBufferCount == 0) {
            for (auto &slot : queue) {
                slot.state = BufferState::Free;
                slot.frameNumber = std::numeric_limits<u32>::max();
                slot.graphicBuffer = nullptr;
            }
        } else if (preallocatedBufferCount < count) {
            state.logger->Warn("Setting the active slot count ({}) higher than the amount of slots with preallocated buffers ({})", count, preallocatedBufferCount);
        }

        activeSlotCount = count;
        bufferEvent->Signal();

        return AndroidStatus::Ok;
    }

    AndroidStatus GraphicBufferProducer::DequeueBuffer(bool async, u32 width, u32 height, AndroidPixelFormat format, u32 usage, i32 &slot, std::optional<AndroidFence> &fence) {
        if ((width && !height) || (!width && height)) {
            state.logger->Warn("Dimensions {}x{} should be uniformly zero or non-zero", width, height);
            return AndroidStatus::BadValue;
        }

        constexpr i32 InvalidGraphicBufferSlot{-1}; //!< https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferQueueCore.h;l=61
        slot = InvalidGraphicBufferSlot;

        std::scoped_lock lock(mutex);
        // We don't need a loop here since the consumer is blocking and instantly frees all buffers
        // If a valid slot is not found on the first iteration then it would be stuck in an infloop
        // As a result of this, we simply warn and return InvalidOperation to the guest
        auto buffer{queue.end()};
        size_t dequeuedSlotCount{};
        for (auto it{queue.begin()}; it != std::min(queue.begin() + activeSlotCount, queue.end()); it++) {
            // We want to select the oldest slot that's free to use as we'd want all slots to be used
            // If we go linearly then we have a higher preference for selecting the former slots and being out of order
            if (it->state == BufferState::Free) {
                if (buffer == queue.end() || it->frameNumber < buffer->frameNumber)
                    buffer = it;
            } else if (it->state == BufferState::Dequeued) {
                dequeuedSlotCount++;
            }
        }

        if (buffer != queue.end()) {
            slot = std::distance(queue.begin(), buffer);
        } else if (async) {
            return AndroidStatus::WouldBlock;
        } else if (dequeuedSlotCount == queue.size()) {
            state.logger->Warn("Client attempting to dequeue more buffers when all buffers are dequeued by the client: {}", dequeuedSlotCount);
            return AndroidStatus::InvalidOperation;
        } else {
            size_t index{};
            std::string bufferString;
            for (auto &bufferSlot : queue)
                bufferString += util::Format("\n#{} - State: {}, Has Graphic Buffer: {}, Frame Number: {}", ++index, ToString(bufferSlot.state), bufferSlot.graphicBuffer != nullptr, bufferSlot.frameNumber);
            state.logger->Warn("Cannot find any free buffers to dequeue:{}", bufferString);
            return AndroidStatus::InvalidOperation;
        }

        width = width ? width : defaultWidth;
        height = height ? height : defaultHeight;
        format = (format != AndroidPixelFormat::None) ? format : defaultFormat;

        if (!buffer->graphicBuffer) {
            // Horizon OS doesn't ever allocate memory for the buffers on the GraphicBufferProducer end
            // All buffers must be preallocated on the client application and attached to an Android buffer using SetPreallocatedBuffer
            return AndroidStatus::NoMemory;
        }
        auto &surface{buffer->graphicBuffer->graphicHandle.surfaces.front()};
        if (buffer->graphicBuffer->format != format || surface.width != width || surface.height != height || (buffer->graphicBuffer->usage & usage) != usage) {
            state.logger->Warn("Buffer which has been dequeued isn't compatible with the supplied parameters: Dimensions: {}x{}={}x{}, Format: {}={}, Usage: 0x{:X}=0x{:X}", width, height, surface.width, surface.height, ToString(format), ToString(buffer->graphicBuffer->format), usage, buffer->graphicBuffer->usage);
            // Nintendo doesn't deallocate the slot which was picked in here and reallocate it as a compatible buffer
            // This is related to the comment above, Nintendo only allocates buffers on the client side
            return AndroidStatus::NoInit;
        }

        buffer->state = BufferState::Dequeued;
        fence = AndroidFence{}; // We just let the presentation engine return a buffer which is ready to be written into, there is no need for further synchronization

        state.logger->Debug("#{} - Dimensions: {}x{}, Format: {}, Usage: 0x{:X}, Is Async: {}", slot, width, height, ToString(format), usage, async);
        return AndroidStatus::Ok;
    }

    AndroidStatus GraphicBufferProducer::DetachBuffer(i32 slot) {
        std::scoped_lock lock(mutex);
        if (slot < 0 || slot >= queue.size()) [[unlikely]] {
            state.logger->Warn("#{} was out of range", slot);
            return AndroidStatus::BadValue;
        }

        auto &bufferSlot{queue[slot]};
        if (bufferSlot.state != BufferState::Dequeued) [[unlikely]] {
            state.logger->Warn("#{} was '{}' instead of being dequeued", slot, ToString(bufferSlot.state));
            return AndroidStatus::BadValue;
        } else if (!bufferSlot.wasBufferRequested) [[unlikely]] {
            state.logger->Warn("#{} was detached prior to being requested", slot);
            return AndroidStatus::BadValue;
        }

        bufferSlot.state = BufferState::Free;
        bufferSlot.frameNumber = std::numeric_limits<u32>::max();
        bufferSlot.graphicBuffer = nullptr;

        bufferEvent->Signal();

        state.logger->Debug("#{}", slot);
        return AndroidStatus::Ok;
    }

    AndroidStatus GraphicBufferProducer::DetachNextBuffer(std::optional<GraphicBuffer> &graphicBuffer, std::optional<AndroidFence> &fence) {
        std::scoped_lock lock(mutex);
        auto bufferSlot{queue.end()};
        for (auto it{queue.begin()}; it != queue.end(); it++) {
            if (it->state == BufferState::Free && it->graphicBuffer) {
                if (bufferSlot == queue.end() || it->frameNumber < bufferSlot->frameNumber)
                    bufferSlot = it;
            }
        }

        if (bufferSlot == queue.end())
            return AndroidStatus::NoMemory;

        bufferSlot->state = BufferState::Free;
        bufferSlot->frameNumber = std::numeric_limits<u32>::max();
        graphicBuffer = *std::exchange(bufferSlot->graphicBuffer, nullptr);
        fence = AndroidFence{};

        bufferEvent->Signal();

        state.logger->Debug("#{}", std::distance(queue.begin(), bufferSlot));
        return AndroidStatus::Ok;
    }

    AndroidStatus GraphicBufferProducer::AttachBuffer(i32 &slot, const GraphicBuffer &graphicBuffer) {
        std::scoped_lock lock(mutex);
        auto bufferSlot{queue.end()};
        for (auto it{queue.begin()}; it != queue.end(); it++) {
            if (it->state == BufferState::Free) {
                if (bufferSlot == queue.end() || it->frameNumber < bufferSlot->frameNumber)
                    bufferSlot = it;
            }
        }

        if (bufferSlot == queue.end()) {
            state.logger->Warn("Could not find any free slots to attach the graphic buffer to");
            return AndroidStatus::NoMemory;
        }

        if (graphicBuffer.magic != GraphicBuffer::Magic)
            throw exception("Unexpected GraphicBuffer magic: 0x{} (Expected: 0x{})", graphicBuffer.magic, GraphicBuffer::Magic);
        else if (graphicBuffer.intCount != sizeof(NvGraphicHandle) / sizeof(u32))
            throw exception("Unexpected GraphicBuffer native_handle integer count: 0x{} (Expected: 0x{})", graphicBuffer.intCount, sizeof(NvGraphicHandle) / sizeof(u32));

        auto &handle{graphicBuffer.graphicHandle};
        if (handle.magic != NvGraphicHandle::Magic)
            throw exception("Unexpected NvGraphicHandle magic: {}", handle.magic);
        else if (handle.surfaceCount < 1)
            throw exception("At least one surface is required in a buffer: {}", handle.surfaceCount);
        else if (handle.surfaceCount > 1)
            throw exception("Multi-planar surfaces are not supported: {}", handle.surfaceCount);

        auto &surface{graphicBuffer.graphicHandle.surfaces.at(0)};
        if (surface.scanFormat != NvDisplayScanFormat::Progressive)
            throw exception("Non-Progressive surfaces are not supported: {}", ToString(surface.scanFormat));
        else if (surface.layout == NvSurfaceLayout::Tiled)
            throw exception("Legacy 16Bx16 tiled surfaces are not supported");

        bufferSlot->state = BufferState::Dequeued;
        bufferSlot->wasBufferRequested = true;
        bufferSlot->isPreallocated = false;
        bufferSlot->graphicBuffer = std::make_unique<GraphicBuffer>(graphicBuffer);

        slot = std::distance(queue.begin(), bufferSlot);

        preallocatedBufferCount = std::count_if(queue.begin(), queue.end(), [](const BufferSlot &slot) { return slot.graphicBuffer && slot.isPreallocated; });
        activeSlotCount = std::count_if(queue.begin(), queue.end(), [](const BufferSlot &slot) { return slot.graphicBuffer != nullptr; });

        state.logger->Debug("#{} - Dimensions: {}x{} [Stride: {}], Format: {}, Layout: {}, {}: {}, Usage: 0x{:X}, NvMap {}: {}, Buffer Start/End: 0x{:X} -> 0x{:X}", slot, surface.width, surface.height, handle.stride, ToString(graphicBuffer.format), ToString(surface.layout), surface.layout == NvSurfaceLayout::Blocklinear ? "Block Height" : "Pitch", surface.layout == NvSurfaceLayout::Blocklinear ? 1U << surface.blockHeightLog2 : surface.pitch, graphicBuffer.usage, surface.nvmapHandle ? "Handle" : "ID", surface.nvmapHandle ? surface.nvmapHandle : handle.nvmapId, surface.offset, surface.offset + surface.size);
        return AndroidStatus::Ok;
    }

    AndroidStatus GraphicBufferProducer::QueueBuffer(i32 slot, i64 timestamp, bool isAutoTimestamp, AndroidRect crop, NativeWindowScalingMode scalingMode, NativeWindowTransform transform, NativeWindowTransform stickyTransform, bool async, u32 swapInterval, const AndroidFence &fence, u32 &width, u32 &height, NativeWindowTransform &transformHint, u32 &pendingBufferCount) {
        switch (scalingMode) {
            case NativeWindowScalingMode::Freeze:
            case NativeWindowScalingMode::ScaleToWindow:
            case NativeWindowScalingMode::ScaleCrop:
            case NativeWindowScalingMode::NoScaleCrop:
                break;

            default:
                state.logger->Warn("{} is not a valid scaling mode", static_cast<u32>(scalingMode));
                return AndroidStatus::BadValue;
        }

        std::scoped_lock lock(mutex);
        if (slot < 0 || slot >= queue.size()) [[unlikely]] {
            state.logger->Warn("#{} was out of range", slot);
            return AndroidStatus::BadValue;
        }

        auto &buffer{queue[slot]};
        if (buffer.state != BufferState::Dequeued) [[unlikely]] {
            state.logger->Warn("#{} was '{}' instead of being dequeued", slot, ToString(buffer.state));
            return AndroidStatus::BadValue;
        } else if (!buffer.wasBufferRequested) [[unlikely]] {
            state.logger->Warn("#{} was queued prior to being requested", slot);
            buffer.wasBufferRequested = true; // Switch ignores this and doesn't return an error, certain homebrew ends up depending on this behavior
        }

        auto graphicBuffer{*buffer.graphicBuffer};
        if (graphicBuffer.width < (crop.right - crop.left) || graphicBuffer.height < (crop.bottom - crop.top)) [[unlikely]] {
            state.logger->Warn("Crop was out of range for surface buffer: ({}-{})x({}-{}) > {}x{}", crop.left, crop.right, crop.top, crop.bottom, graphicBuffer.width, graphicBuffer.height);
            return AndroidStatus::BadValue;
        }

        if (!buffer.texture) [[unlikely]] {
            // We lazily create a texture if one isn't present at queue time, this allows us to look up the texture in the texture cache
            // If we deterministically know that the texture is written by the CPU then we can allocate a CPU-shared host texture for fast uploads
            gpu::texture::Format format;
            switch (graphicBuffer.format) {
                case AndroidPixelFormat::RGBA8888:
                case AndroidPixelFormat::RGBX8888:
                    format = gpu::format::RGBA8888Unorm;
                    break;

                case AndroidPixelFormat::RGB565:
                    format = gpu::format::RGB565Unorm;
                    break;

                default:
                    throw exception("Unknown format in buffer: '{}' ({})", ToString(graphicBuffer.format), static_cast<u32>(graphicBuffer.format));
            }

            auto &handle{graphicBuffer.graphicHandle};
            if (handle.magic != NvGraphicHandle::Magic)
                throw exception("Unexpected NvGraphicHandle magic: {}", handle.surfaceCount);
            else if (handle.surfaceCount < 1)
                throw exception("At least one surface is required in a buffer: {}", handle.surfaceCount);
            else if (handle.surfaceCount > 1)
                throw exception("Multi-planar surfaces are not supported: {}", handle.surfaceCount);

            auto &surface{graphicBuffer.graphicHandle.surfaces.at(0)};
            if (surface.scanFormat != NvDisplayScanFormat::Progressive)
                throw exception("Non-Progressive surfaces are not supported: {}", ToString(surface.scanFormat));

            std::shared_ptr<nvdrv::device::NvMap::NvMapObject> nvBuffer{};
            {
                auto driver{nvdrv::driver.lock()};
                auto nvmap{driver->nvMap.lock()};
                if (surface.nvmapHandle) {
                    nvBuffer = nvmap->GetObject(surface.nvmapHandle);
                } else {
                    std::shared_lock nvmapLock(nvmap->mapMutex);
                    for (const auto &object : nvmap->maps) {
                        if (object->id == handle.nvmapId) {
                            nvBuffer = object;
                            break;
                        }
                    }
                    if (!nvBuffer)
                        throw exception("A QueueBuffer request has an invalid NvMap Handle ({}) and ID ({})", surface.nvmapHandle, handle.nvmapId);
                }
            }

            if (surface.size > (nvBuffer->size - surface.offset))
                throw exception("Surface doesn't fit into NvMap mapping of size 0x{:X} when mapped at 0x{:X} -> 0x{:X}", nvBuffer->size, surface.offset, surface.offset + surface.size);

            gpu::texture::TileMode tileMode;
            gpu::texture::TileConfig tileConfig{};
            if (surface.layout == NvSurfaceLayout::Blocklinear) {
                tileMode = gpu::texture::TileMode::Block;
                tileConfig = {
                    .surfaceWidth = static_cast<u16>(surface.width),
                    .blockHeight = static_cast<u8>(1U << surface.blockHeightLog2),
                    .blockDepth = 1,
                };
            } else if (surface.layout == NvSurfaceLayout::Pitch) {
                tileMode = gpu::texture::TileMode::Pitch;
                tileConfig = {
                    .pitch = surface.pitch,
                };
            } else if (surface.layout == NvSurfaceLayout::Tiled) {
                throw exception("Legacy 16Bx16 tiled surfaces are not supported");
            }

            auto guestTexture{std::make_shared<gpu::GuestTexture>(state, nvBuffer->ptr + surface.offset, gpu::texture::Dimensions(surface.width, surface.height), format, tileMode, tileConfig)};
            buffer.texture = guestTexture->CreateTexture({}, vk::ImageTiling::eLinear);
        }

        switch (transform) {
            case NativeWindowTransform::Identity:
            case NativeWindowTransform::MirrorHorizontal:
            case NativeWindowTransform::MirrorVertical:
            case NativeWindowTransform::Rotate90:
            case NativeWindowTransform::Rotate180:
            case NativeWindowTransform::Rotate270:
            case NativeWindowTransform::MirrorHorizontalRotate90:
            case NativeWindowTransform::MirrorVerticalRotate90:
            case NativeWindowTransform::InvertDisplay:
                break;

            default:
                throw exception("Application attempting to perform unknown transformation: {:#b}", static_cast<u32>(transform));
        }

        switch (stickyTransform) {
            // Note: Sticky transforms are a legacy feature and aren't implemented in HOS nor the Android version it is based on, they are effectively inert
            // Certain games will still pass in values for sticky transforms (even if they don't do anything), we should not assert on these and verify their validity
            case NativeWindowTransform::Identity:
            case NativeWindowTransform::MirrorHorizontal:
            case NativeWindowTransform::MirrorVertical:
            case NativeWindowTransform::Rotate90:
            case NativeWindowTransform::Rotate180:
            case NativeWindowTransform::Rotate270:
            case NativeWindowTransform::MirrorHorizontalRotate90:
            case NativeWindowTransform::MirrorVerticalRotate90:
            case NativeWindowTransform::InvertDisplay:
                break;

            default:
                throw exception("Application attempting to perform unknown sticky transformation: {:#b}", static_cast<u32>(stickyTransform));
        }

        fence.Wait(state.soc->host1x);

        {
            auto &texture{buffer.texture};
            std::scoped_lock textureLock(*texture);
            texture->SynchronizeHost();
            u64 frameId;
            state.gpu->presentation.Present(texture, isAutoTimestamp ? 0 : timestamp, swapInterval, crop, scalingMode, transform, frameId);
        }

        buffer.frameNumber = ++frameNumber;
        buffer.state = BufferState::Free;
        bufferEvent->Signal();

        width = defaultWidth;
        height = defaultHeight;
        transformHint = state.gpu->presentation.GetTransformHint();
        pendingBufferCount = GetPendingBufferCount();

        state.logger->Debug("#{} - {}Timestamp: {}, Crop: ({}-{})x({}-{}), Scale Mode: {}, Transform: {} [Sticky: {}], Swap Interval: {}, Is Async: {}", slot, isAutoTimestamp ? "Auto " : "", timestamp, crop.left, crop.right, crop.top, crop.bottom, ToString(scalingMode), ToString(transform), ToString(stickyTransform), swapInterval, async);
        return AndroidStatus::Ok;
    }

    void GraphicBufferProducer::CancelBuffer(i32 slot, const AndroidFence &fence) {
        std::scoped_lock lock(mutex);
        if (slot < 0 || slot >= queue.size()) [[unlikely]] {
            state.logger->Warn("#{} was out of range", slot);
            return;
        }

        auto &buffer{queue[slot]};
        if (buffer.state != BufferState::Dequeued) [[unlikely]] {
            state.logger->Warn("#{} is not owned by the producer as it is '{}' instead of being dequeued", slot, ToString(buffer.state));
            return;
        }

        fence.Wait(state.soc->host1x);

        buffer.state = BufferState::Free;
        buffer.frameNumber = 0;
        bufferEvent->Signal();

        state.logger->Debug("#{}", slot);
    }

    AndroidStatus GraphicBufferProducer::Query(NativeWindowQuery query, u32 &out) {
        std::scoped_lock lock(mutex);
        switch (query) {
            case NativeWindowQuery::Width:
                out = defaultWidth;
                break;

            case NativeWindowQuery::Height:
                out = defaultHeight;
                break;

            case NativeWindowQuery::Format:
                out = static_cast<u32>(defaultFormat);
                break;

            case NativeWindowQuery::MinUndequeuedBuffers:
                // Calls into BufferQueueCore::getMinUndequeuedBufferCountLocked, which always returns mMaxAcquiredBufferCount (0) on HOS as UseAsyncBuffer is false due to HOS not using asynchronous buffers (No allocations on the server are supported)
                // https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/BufferQueueCore.cpp;l=133-145
                out = 0;
                break;

            case NativeWindowQuery::StickyTransform:
                out = static_cast<u32>(NativeWindowTransform::Identity); // We don't support any sticky transforms, they're only used by the LEGACY camera mode
                break;

            case NativeWindowQuery::ConsumerRunningBehind:
                out = false; // We have no way of knowing if the consumer is slower than the producer as we are not notified when a buffer has been acquired on the host
                break;

            case NativeWindowQuery::ConsumerUsageBits:
                out = 0; // HOS layers (Consumers) have no Gralloc usage bits set
                break;

            case NativeWindowQuery::MaxBufferCount: {
                // Calls into BufferQueueCore::getMaxBufferCountLocked, which will always return mDefaultMaxBufferCount (2 which is activeBufferCount's initial value) or mOverrideMaxBufferCount (activeBufferCount) as it's set during SetPreallocatedBuffer
                // https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/BufferQueueCore.cpp;l=151-172
                out = activeSlotCount;
                break;
            }

            default:
                state.logger->Warn("Query not supported: {}", static_cast<u32>(query));
                return AndroidStatus::BadValue;
        }

        state.logger->Debug("{}: {}", ToString(query), out);
        return AndroidStatus::Ok;
    }

    AndroidStatus GraphicBufferProducer::Connect(NativeWindowApi api, bool producerControlledByApp, u32 &width, u32 &height, NativeWindowTransform &transformHint, u32 &pendingBufferCount) {
        std::scoped_lock lock(mutex);
        if (connectedApi != NativeWindowApi::None) [[unlikely]] {
            state.logger->Warn("Already connected to API '{}' while connection to '{}' is requested", ToString(connectedApi), ToString(api));
            return AndroidStatus::BadValue;
        }

        switch (api) {
            case NativeWindowApi::EGL:
            case NativeWindowApi::CPU:
            case NativeWindowApi::Media:
            case NativeWindowApi::Camera:
                break;

            default:
                state.logger->Warn("Unknown API: {}", static_cast<u32>(api));
                return AndroidStatus::BadValue;
        }

        connectedApi = api;
        width = defaultWidth;
        height = defaultHeight;
        transformHint = state.gpu->presentation.GetTransformHint();
        pendingBufferCount = GetPendingBufferCount();

        state.logger->Debug("API: {}, Producer Controlled By App: {}, Default Dimensions: {}x{}, Transform Hint: {}, Pending Buffer Count: {}", ToString(api), producerControlledByApp, width, height, ToString(transformHint), pendingBufferCount);
        return AndroidStatus::Ok;
    }

    AndroidStatus GraphicBufferProducer::Disconnect(NativeWindowApi api) {
        std::scoped_lock lock(mutex);

        switch (api) {
            case NativeWindowApi::EGL:
            case NativeWindowApi::CPU:
            case NativeWindowApi::Media:
            case NativeWindowApi::Camera:
                break;

            default:
                state.logger->Warn("Unknown API: {}", static_cast<u32>(api));
                return AndroidStatus::BadValue;
        }

        if (api != connectedApi) {
            state.logger->Warn("Disconnecting from API '{}' while connected to '{}'", ToString(api), ToString(connectedApi));
            return AndroidStatus::BadValue;
        }

        connectedApi = NativeWindowApi::None;
        for (auto &slot : queue) {
            slot.state = BufferState::Free;
            slot.frameNumber = std::numeric_limits<u32>::max();
            slot.graphicBuffer = nullptr;
        }

        state.logger->Debug("API: {}", ToString(api));
        return AndroidStatus::Ok;
    }

    AndroidStatus GraphicBufferProducer::SetPreallocatedBuffer(i32 slot, const GraphicBuffer *graphicBuffer) {
        std::scoped_lock lock(mutex);
        if (slot < 0 || slot >= MaxSlotCount) [[unlikely]] {
            state.logger->Warn("#{} was out of range", slot);
            return AndroidStatus::BadValue;
        }

        auto &buffer{queue[slot]};
        buffer.state = BufferState::Free;
        buffer.frameNumber = 0;
        buffer.wasBufferRequested = false;
        buffer.isPreallocated = graphicBuffer != nullptr;
        buffer.graphicBuffer = graphicBuffer ? std::make_unique<GraphicBuffer>(*graphicBuffer) : nullptr;
        buffer.texture = {};

        if (graphicBuffer) {
            if (graphicBuffer->magic != GraphicBuffer::Magic)
                throw exception("Unexpected GraphicBuffer magic: 0x{} (Expected: 0x{})", graphicBuffer->magic, GraphicBuffer::Magic);
            else if (graphicBuffer->intCount != sizeof(NvGraphicHandle) / sizeof(u32))
                throw exception("Unexpected GraphicBuffer native_handle integer count: 0x{} (Expected: 0x{})", graphicBuffer->intCount, sizeof(NvGraphicHandle));

            auto &handle{graphicBuffer->graphicHandle};
            if (handle.magic != NvGraphicHandle::Magic)
                throw exception("Unexpected NvGraphicHandle magic: {}", handle.surfaceCount);
            else if (handle.surfaceCount < 1)
                throw exception("At least one surface is required in a buffer: {}", handle.surfaceCount);
            else if (handle.surfaceCount > 1)
                throw exception("Multi-planar surfaces are not supported: {}", handle.surfaceCount);

            auto &surface{graphicBuffer->graphicHandle.surfaces.at(0)};
            if (surface.scanFormat != NvDisplayScanFormat::Progressive)
                throw exception("Non-Progressive surfaces are not supported: {}", ToString(surface.scanFormat));
            else if (surface.layout == NvSurfaceLayout::Tiled)
                throw exception("Legacy 16Bx16 tiled surfaces are not supported");

            state.logger->Debug("#{} - Dimensions: {}x{} [Stride: {}], Format: {}, Layout: {}, {}: {}, Usage: 0x{:X}, NvMap {}: {}, Buffer Start/End: 0x{:X} -> 0x{:X}", slot, surface.width, surface.height, handle.stride, ToString(graphicBuffer->format), ToString(surface.layout), surface.layout == NvSurfaceLayout::Blocklinear ? "Block Height" : "Pitch", surface.layout == NvSurfaceLayout::Blocklinear ? 1U << surface.blockHeightLog2 : surface.pitch, graphicBuffer->usage, surface.nvmapHandle ? "Handle" : "ID", surface.nvmapHandle ? surface.nvmapHandle : handle.nvmapId, surface.offset, surface.offset + surface.size);
        } else {
            state.logger->Debug("#{} - No GraphicBuffer", slot);
        }

        preallocatedBufferCount = std::count_if(queue.begin(), queue.end(), [](const BufferSlot &slot) { return slot.graphicBuffer && slot.isPreallocated; });
        activeSlotCount = std::count_if(queue.begin(), queue.end(), [](const BufferSlot &slot) { return slot.graphicBuffer != nullptr; });

        bufferEvent->Signal();

        return AndroidStatus::Ok;
    }

    void GraphicBufferProducer::OnTransact(TransactionCode code, Parcel &in, Parcel &out) {
        switch (code) {
            case TransactionCode::RequestBuffer: {
                GraphicBuffer *buffer{};
                auto result{RequestBuffer(in.Pop<i32>(), buffer)};
                out.PushOptionalFlattenable(buffer);
                out.Push(result);
                break;
            }

            case TransactionCode::SetBufferCount: {
                auto result{SetBufferCount(in.Pop<i32>())};
                out.Push(result);
                break;
            }

            case TransactionCode::DequeueBuffer: {
                i32 slot{};
                std::optional<AndroidFence> fence{};
                auto result{DequeueBuffer(in.Pop<u32>(), in.Pop<u32>(), in.Pop<u32>(), in.Pop<AndroidPixelFormat>(), in.Pop<u32>(), slot, fence)};
                out.Push(slot);
                out.PushOptionalFlattenable(fence);
                out.Push(result);
                break;
            }

            case TransactionCode::DetachBuffer: {
                auto result{DetachBuffer(in.Pop<i32>())};
                out.Push(result);
                break;
            }

            case TransactionCode::DetachNextBuffer: {
                std::optional<GraphicBuffer> graphicBuffer{};
                std::optional<AndroidFence> fence{};
                auto result{DetachNextBuffer(graphicBuffer, fence)};
                out.PushOptionalFlattenable(graphicBuffer);
                out.PushOptionalFlattenable(fence);
                out.Push(result);
                break;
            }

            case TransactionCode::AttachBuffer: {
                i32 slotOut{};
                auto result{AttachBuffer(slotOut, in.Pop<GraphicBuffer>())};
                out.Push(slotOut);
                out.Push(result);
                break;
            }

            case TransactionCode::QueueBuffer: {
                u32 width{}, height{}, pendingBufferCount{};
                NativeWindowTransform transformHint{};

                constexpr u64 QueueBufferInputSize{0x54}; //!< The size of the QueueBufferInput structure (https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/IGraphicBufferProducer.h;l=265-315)

                auto slot{in.Pop<i32>()};
                auto queueBufferInputSize{in.Pop<u64>()};
                if (queueBufferInputSize != QueueBufferInputSize)
                    throw exception("The size of QueueBufferInput in the Parcel (0x{:X}) doesn't match the expected size (0x{:X})", queueBufferInputSize, QueueBufferInputSize);
                auto result{QueueBuffer(slot, in.Pop<i64>(), in.Pop<u32>(), in.Pop<AndroidRect>(), in.Pop<NativeWindowScalingMode>(), in.Pop<NativeWindowTransform>(), in.Pop<NativeWindowTransform>(), in.Pop<u32>(), in.Pop<u32>(), in.Pop<AndroidFence>(), width, height, transformHint, pendingBufferCount)};

                out.Push(width);
                out.Push(height);
                out.Push(transformHint);
                out.Push(pendingBufferCount);
                out.Push(result);
                break;
            }

            case TransactionCode::CancelBuffer: {
                CancelBuffer(in.Pop<i32>(), in.Pop<AndroidFence>());
                break;
            }

            case TransactionCode::Query: {
                u32 queryOut{};
                auto result{Query(in.Pop<NativeWindowQuery>(), queryOut)};
                out.Push(queryOut);
                out.Push(result);
                break;
            }

            case TransactionCode::Connect: {
                bool hasProducerListener{in.Pop<u32>() != 0};
                if (hasProducerListener)
                    throw exception("Callbacks using IProducerListener are not supported");

                u32 width{}, height{}, pendingBufferCount{};
                NativeWindowTransform transformHint{};
                auto result{Connect(in.Pop<NativeWindowApi>(), in.Pop<u32>(), width, height, transformHint, pendingBufferCount)};
                out.Push(width);
                out.Push(height);
                out.Push(transformHint);
                out.Push(pendingBufferCount);
                out.Push(result);
                break;
            }

            case TransactionCode::Disconnect: {
                auto result{Disconnect(in.Pop<NativeWindowApi>())};
                out.Push(result);
                break;
            }

            case TransactionCode::SetPreallocatedBuffer: {
                auto result{SetPreallocatedBuffer(in.Pop<i32>(), in.PopOptionalFlattenable<GraphicBuffer>())};
                out.Push(result);
                break;
            }

            default:
                throw exception("An unimplemented transaction was called: {}", static_cast<u32>(code));
        }
    }
}
