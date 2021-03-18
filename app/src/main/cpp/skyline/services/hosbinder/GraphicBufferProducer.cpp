// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/hardware_buffer.h>
#include <gpu.h>
#include <gpu/format.h>
#include <services/nvdrv/driver.h>
#include <services/nvdrv/devices/nvmap.h>
#include <services/common/fence.h>
#include "GraphicBufferProducer.h"

namespace skyline::service::hosbinder {
    Buffer::Buffer(const GbpBuffer &gbpBuffer, std::shared_ptr<gpu::Texture> texture) : gbpBuffer(gbpBuffer), texture(std::move(texture)) {}

    GraphicBufferProducer::GraphicBufferProducer(const DeviceState &state) : state(state) {}

    void GraphicBufferProducer::RequestBuffer(Parcel &in, Parcel &out) {
        u32 slot{in.Pop<u32>()};

        out.Push<u32>(1);
        out.Push<u32>(sizeof(GbpBuffer));
        out.Push<u32>(0);
        out.Push(queue.at(slot)->gbpBuffer);

        state.logger->Debug("Slot: {}", slot, sizeof(GbpBuffer));
    }

    void GraphicBufferProducer::DequeueBuffer(Parcel &in, Parcel &out) {
        in.Pop<u32>(); //!< async boolean flag
        u32 width{in.Pop<u32>()};
        u32 height{in.Pop<u32>()};
        u32 format{in.Pop<u32>()};
        u32 usage{in.Pop<u32>()};

        std::optional<u32> slot{std::nullopt};
        while (!slot) {
            for (auto &buffer : queue) {
                if (buffer.second->status == BufferStatus::Free && (format == 0 || buffer.second->gbpBuffer.format == format) && buffer.second->gbpBuffer.width == width && buffer.second->gbpBuffer.height == height && (buffer.second->gbpBuffer.usage & usage) == usage) {
                    slot = buffer.first;
                    buffer.second->status = BufferStatus::Dequeued;
                    break;
                }
            }
        }

        out.Push(*slot);
        out.Push(std::array<u32, 13>{1, 0x24}); // Unknown

        state.logger->Debug("Width: {}, Height: {}, Format: {}, Usage: {}, Slot: {}", width, height, format, usage, *slot);
    }

    void GraphicBufferProducer::QueueBuffer(Parcel &in, Parcel &out) {
        struct Data {
            u32 slot;
            u64 timestamp;
            u32 autoTimestamp;
            ARect crop;
            u32 scalingMode;
            u32 transform;
            u32 stickyTransform;
            u64 _unk0_;
            u32 swapInterval;
            std::array<nvdrv::Fence, 4> fence;
        } &data = in.Pop<Data>();

        auto buffer{queue.at(data.slot)};
        buffer->status = BufferStatus::Queued;

        buffer->texture->SynchronizeHost();
        state.gpu->presentation.Present(buffer->texture);
        queue.at(data.slot)->status = BufferStatus::Free;
        state.gpu->presentation.bufferEvent->Signal();

        struct {
            u32 width;
            u32 height;
            u32 _pad_[3];
        } output{
            .width = buffer->gbpBuffer.width,
            .height = buffer->gbpBuffer.height,
        };
        out.Push(output);

        state.logger->Debug("Timestamp: {}, Auto Timestamp: {}, Crop: [T: {}, B: {}, L: {}, R: {}], Scaling Mode: {}, Transform: {}, Sticky Transform: {}, Swap Interval: {}, Slot: {}", data.timestamp, data.autoTimestamp, data.crop.top, data.crop.bottom, data.crop.left, data.crop.right, data.scalingMode, data.transform, data.stickyTransform, data.swapInterval, data.slot);
    }

    void GraphicBufferProducer::CancelBuffer(Parcel &in) {
        u32 slot{in.Pop<u32>()};
        //auto fences{in.Pop<std::array<nvdrv::Fence, 4>>()};

        queue.at(slot)->status = BufferStatus::Free;

        state.logger->Debug("Slot: {}", slot);
    }

    void GraphicBufferProducer::Connect(Parcel &out) {
        struct {
            u32 width{constant::HandheldResolutionW}; //!< The width of the display
            u32 height{constant::HandheldResolutionH}; //!< The height of the display
            u32 transformHint{}; //!< A hint of the transformation of the display
            u32 pendingBuffers{}; //!< The number of pending buffers
            u32 status{}; //!< The status of the buffer queue
        } data{};
        out.Push(data);
        state.logger->Debug("Connect");
    }

    void GraphicBufferProducer::SetPreallocatedBuffer(Parcel &in) {
        struct Data {
            u32 slot;
            u32 _unk0_;
            u32 length;
            u32 _pad0_;
        } &data = in.Pop<Data>();

        auto &gbpBuffer{in.Pop<GbpBuffer>()};

        std::shared_ptr<nvdrv::device::NvMap::NvMapObject> nvBuffer{};

        auto driver{nvdrv::driver.lock()};
        auto nvmap{driver->nvMap.lock()};

        if (gbpBuffer.nvmapHandle) {
            nvBuffer = nvmap->GetObject(gbpBuffer.nvmapHandle);
        } else {
            std::shared_lock nvmapLock(nvmap->mapMutex);
            for (const auto &object : nvmap->maps) {
                if (object->id == gbpBuffer.nvmapId) {
                    nvBuffer = object;
                    break;
                }
            }
            if (!nvBuffer)
                throw exception("A QueueBuffer request has an invalid NVMap Handle ({}) and ID ({})", gbpBuffer.nvmapHandle, gbpBuffer.nvmapId);
        }

        gpu::texture::Format format;
        switch (gbpBuffer.format) {
            case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
            case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
                format = gpu::format::RGBA8888Unorm;
                break;
            case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM:
                format = gpu::format::RGB565Unorm;
                break;
            default:
                throw exception("Unknown pixel format used for FB");
        }

        auto texture{std::make_shared<gpu::GuestTexture>(state, nvBuffer->ptr + gbpBuffer.offset, gpu::texture::Dimensions(gbpBuffer.width, gbpBuffer.height), format, gpu::texture::TileMode::Block, gpu::texture::TileConfig{.surfaceWidth = static_cast<u16>(gbpBuffer.stride), .blockHeight = static_cast<u8>(1U << gbpBuffer.blockHeightLog2), .blockDepth = 1})};

        queue[data.slot] = std::make_shared<Buffer>(gbpBuffer, texture->InitializeTexture());
        state.gpu->presentation.bufferEvent->Signal();

        state.logger->Debug("Slot: {}, Magic: 0x{:X}, Width: {}, Height: {}, Stride: {}, Format: {}, Usage: {}, Index: {}, ID: {}, Handle: {}, Offset: 0x{:X}, Block Height: {}, Size: 0x{:X}", data.slot, gbpBuffer.magic, gbpBuffer.width, gbpBuffer.height, gbpBuffer.stride, gbpBuffer.format, gbpBuffer.usage, gbpBuffer.index, gbpBuffer.nvmapId, gbpBuffer.nvmapHandle, gbpBuffer.offset, (1U << gbpBuffer.blockHeightLog2), gbpBuffer.size);
    }

    void GraphicBufferProducer::OnTransact(TransactionCode code, Parcel &in, Parcel &out) {
        switch (code) {
            case TransactionCode::RequestBuffer:
                RequestBuffer(in, out);
                break;
            case TransactionCode::DequeueBuffer:
                DequeueBuffer(in, out);
                break;
            case TransactionCode::QueueBuffer:
                QueueBuffer(in, out);
                break;
            case TransactionCode::CancelBuffer:
                CancelBuffer(in);
                break;
            case TransactionCode::Query:
                out.Push<u64>(0);
                break;
            case TransactionCode::Connect:
                Connect(out);
                break;
            case TransactionCode::Disconnect:
                break;
            case TransactionCode::SetPreallocatedBuffer:
                SetPreallocatedBuffer(in);
                break;
            default:
                throw exception("An unimplemented transaction was called: {}", static_cast<u32>(code));
        }
    }

    static frz::unordered_map<frz::string, DisplayId, 5> DisplayTypeMap{
        {"Default", DisplayId::Default},
        {"External", DisplayId::External},
        {"Edid", DisplayId::Edid},
        {"Internal", DisplayId::Internal},
        {"Null", DisplayId::Null},
    };

    void GraphicBufferProducer::SetDisplay(const std::string &name) {
        try {
            if (displayId == DisplayId::Null)
                displayId = DisplayTypeMap.at(frz::string(name.data(), name.size()));
            else
                throw exception("Trying to change display type from non-null type");
        } catch (const std::out_of_range &) {
            throw exception("The display with name: '{}' doesn't exist", name);
        }
    }

    void GraphicBufferProducer::CloseDisplay() {
        if (displayId == DisplayId::Null)
            state.logger->Warn("Trying to close uninitiated display");
        displayId = DisplayId::Null;
    }

    std::weak_ptr<GraphicBufferProducer> producer{};
}
