// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <kernel/memory.h>
#include <kernel/types/KProcess.h>
#include <common/trace.h>
#include "buffer.h"

namespace skyline::gpu {
    void Buffer::TryEnableMegaBuffering() {
        megaBufferOffset = 0;
        megaBufferingEnabled = backing.size() < MegaBufferingDisableThreshold;
    }

    void Buffer::SetupGuestMappings() {
        u8 *alignedData{util::AlignDown(guest->data(), PAGE_SIZE)};
        size_t alignedSize{static_cast<size_t>(util::AlignUp(guest->data() + guest->size(), PAGE_SIZE) - alignedData)};

        alignedMirror = gpu.state.process->memory.CreateMirror(alignedData, alignedSize);
        mirror = alignedMirror.subspan(static_cast<size_t>(guest->data() - alignedData), guest->size());

        trapHandle = gpu.state.nce->TrapRegions(*guest, true, [this] {
            std::lock_guard lock(*this);
            SynchronizeGuest(true); // We can skip trapping since the caller will do it
            WaitOnFence();
        }, [this] {
            std::lock_guard lock(*this);
            SynchronizeGuest(true);
            dirtyState = DirtyState::CpuDirty; // We need to assume the buffer is dirty since we don't know what the guest is writing
            WaitOnFence();
        });
    }

    Buffer::Buffer(GPU &gpu, GuestBuffer guest) : gpu(gpu), backing(gpu.memory.AllocateBuffer(guest.size())), guest(guest) {
        TryEnableMegaBuffering();
        SetupGuestMappings();
    }

    Buffer::Buffer(GPU &gpu, const std::shared_ptr<FenceCycle> &pCycle, GuestBuffer guest, span<std::shared_ptr<Buffer>> srcBuffers) : gpu(gpu), backing(gpu.memory.AllocateBuffer(guest.size())), guest(guest) {
        std::scoped_lock bufLock{*this};

        TryEnableMegaBuffering();
        SetupGuestMappings();

        // Source buffers don't necessarily fully overlap with us so we have to perform a sync here to prevent any gaps
        SynchronizeHost(false);

        // Copies between two buffers based off of their mappings in guest memory
        auto copyBuffer{[](auto dstGuest, auto srcGuest, auto dstPtr, auto srcPtr) {
            if (dstGuest.begin().base() <= srcGuest.begin().base()) {
                size_t dstOffset{static_cast<size_t>(srcGuest.begin().base() - dstGuest.begin().base())};
                size_t copySize{std::min(dstGuest.size() - dstOffset, srcGuest.size())};
                std::memcpy(dstPtr + dstOffset, srcPtr, copySize);
            } else if (dstGuest.begin().base() > srcGuest.begin().base()) {
                size_t srcOffset{static_cast<size_t>(dstGuest.begin().base() - srcGuest.begin().base())};
                size_t copySize{std::min(dstGuest.size(), srcGuest.size() - srcOffset)};
                std::memcpy(dstPtr, srcPtr + srcOffset, copySize);
            }
        }};

        // Transfer data/state from source buffers
        for (const auto &srcBuffer : srcBuffers) {
            std::scoped_lock lock{*srcBuffer};
            if (srcBuffer->guest) {
                if (!srcBuffer->megaBufferingEnabled)
                    megaBufferingEnabled = false;

                if (srcBuffer->dirtyState == Buffer::DirtyState::GpuDirty) {
                    // If the source buffer is GPU dirty we cannot directly copy over its GPU backing contents

                    // Only sync back the buffer if it's not attched to the current fence cycle, otherwise propagate the GPU dirtiness
                    if (!srcBuffer->cycle.owner_before(pCycle)) {
                        // Perform a GPU -> CPU sync on the source then do a CPU -> GPU sync for the region occupied by the source
                        // This is required since if we were created from a two buffers: one GPU dirty in the current cycle, and one GPU dirty in the previous cycle, if we marked ourselves as CPU dirty here then the GPU dirtiness from the current cycle buffer would be ignored and cause writes to be missed
                        srcBuffer->SynchronizeGuest(true);
                        copyBuffer(guest, *srcBuffer->guest, backing.data(), srcBuffer->mirror.data());
                    } else {
                        MarkGpuDirty();
                    }
                } else if (srcBuffer->dirtyState == Buffer::DirtyState::Clean) {
                    // For clean buffers we can just copy over the GPU backing data directly
                    // This is necessary since clean buffers may not have matching GPU/CPU data in the case of non-megabuffered inline updates
                    copyBuffer(guest, *srcBuffer->guest, backing.data(), srcBuffer->backing.data());
                }

                // CPU dirty buffers are already synchronized in the initial SynchronizeHost call so don't need special handling
            }
        }
    }

    Buffer::Buffer(GPU &gpu, vk::DeviceSize size) : gpu(gpu), backing(gpu.memory.AllocateBuffer(size)) {
        TryEnableMegaBuffering();
        dirtyState = DirtyState::Clean; // Since this is a host-only buffer it's always going to be clean
    }

    Buffer::~Buffer() {
        std::lock_guard lock(*this);
        if (trapHandle)
            gpu.state.nce->DeleteTrap(*trapHandle);
        SynchronizeGuest(true);
        if (alignedMirror.valid())
            munmap(alignedMirror.data(), alignedMirror.size());
    }

    void Buffer::MarkGpuDirty() {
        if (dirtyState == DirtyState::GpuDirty || !guest)
            return;

        megaBufferingEnabled = false; // We can no longer megabuffer this buffer after it has been written by the GPU
        gpu.state.nce->RetrapRegions(*trapHandle, false);
        dirtyState = DirtyState::GpuDirty;
    }

    void Buffer::WaitOnFence() {
        TRACE_EVENT("gpu", "Buffer::WaitOnFence");

        auto lCycle{cycle.lock()};
        if (lCycle) {
            lCycle->Wait();
            cycle.reset();
        }
    }

    bool Buffer::PollFence() {
        auto lCycle{cycle.lock()};
        if (lCycle && lCycle->Poll()) {
            cycle.reset();
            return true;
        }
        return false;
    }

    void Buffer::SynchronizeHost(bool rwTrap) {
        if (dirtyState != DirtyState::CpuDirty || !guest)
            return; // If the buffer has not been modified on the CPU or there's no guest buffer, there is no need to synchronize it

        WaitOnFence();

        TRACE_EVENT("gpu", "Buffer::SynchronizeHost");

        // If we have performed a CPU->GPU sync and megabuffering is enabled for this buffer the megabuffer copy of the buffer will no longer be up-to-date
        InvalidateMegaBuffer();

        std::memcpy(backing.data(), mirror.data(), mirror.size());

        if (rwTrap) {
            megaBufferingEnabled = false; // We can't megabuffer a buffer written by the GPU
            gpu.state.nce->RetrapRegions(*trapHandle, false);
            dirtyState = DirtyState::GpuDirty;
        } else {
            gpu.state.nce->RetrapRegions(*trapHandle, true);
            dirtyState = DirtyState::Clean;
        }
    }

    void Buffer::SynchronizeHostWithCycle(const std::shared_ptr<FenceCycle> &pCycle, bool rwTrap) {
        if (dirtyState != DirtyState::CpuDirty || !guest)
            return;

        if (!cycle.owner_before(pCycle))
            WaitOnFence();

        TRACE_EVENT("gpu", "Buffer::SynchronizeHostWithCycle");

        // If we have performed a CPU->GPU sync and megabuffering is enabled for this buffer the megabuffer copy of the buffer will no longer be up-to-date so force a recreation
        InvalidateMegaBuffer();

        std::memcpy(backing.data(), mirror.data(), mirror.size());

        if (rwTrap) {
            megaBufferingEnabled = false; // We can't megabuffer a buffer written by the GPU
            gpu.state.nce->RetrapRegions(*trapHandle, false);
            dirtyState = DirtyState::GpuDirty;
        } else {
            gpu.state.nce->RetrapRegions(*trapHandle, true);
            dirtyState = DirtyState::Clean;
        }
    }

    void Buffer::SynchronizeGuest(bool skipTrap, bool nonBlocking) {
        if (dirtyState != DirtyState::GpuDirty || !guest)
            return; // If the buffer has not been used on the GPU or there's no guest buffer, there is no need to synchronize it

        if (nonBlocking && !PollFence())
            return;
        else if (!nonBlocking)
            WaitOnFence();

        TRACE_EVENT("gpu", "Buffer::SynchronizeGuest");

        std::memcpy(mirror.data(), backing.data(), mirror.size());

        if (!skipTrap)
            gpu.state.nce->RetrapRegions(*trapHandle, true);

        dirtyState = DirtyState::Clean;
        TryEnableMegaBuffering(); // If megaBuffering was disabled due to potential GPU dirtiness we can safely try to re-enable it now that the buffer is clean
    }

    /**
     * @brief A FenceCycleDependency that synchronizes the contents of a host buffer with the guest buffer
     */
    struct BufferGuestSync : public FenceCycleDependency {
        std::shared_ptr<Buffer> buffer;

        explicit BufferGuestSync(std::shared_ptr<Buffer> buffer) : buffer(std::move(buffer)) {}

        ~BufferGuestSync() {
            TRACE_EVENT("gpu", "Buffer::BufferGuestSync");
            buffer->SynchronizeGuest();
        }
    };

    void Buffer::SynchronizeGuestWithCycle(const std::shared_ptr<FenceCycle> &pCycle) {
        if (!cycle.owner_before(pCycle))
            WaitOnFence();

        pCycle->AttachObject(std::make_shared<BufferGuestSync>(shared_from_this()));
        cycle = pCycle;
    }

    void Buffer::Read(const std::shared_ptr<FenceCycle> &pCycle, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize offset) {
        if (dirtyState == DirtyState::CpuDirty || dirtyState == DirtyState::Clean) {
            std::memcpy(data.data(), mirror.data() + offset, data.size());
        } else if (dirtyState == DirtyState::GpuDirty) {
            // If this buffer was attached to the current cycle, flush all pending host GPU work and wait to ensure that we read valid data
            if (!cycle.owner_before(pCycle))
                flushHostCallback();

            SynchronizeGuest();

            std::memcpy(data.data(), backing.data() + offset, data.size());
        }
    }

    void Buffer::Write(const std::shared_ptr<FenceCycle> &pCycle, const std::function<void()> &flushHostCallback, const std::function<void()> &gpuCopyCallback, span<u8> data, vk::DeviceSize offset) {
        InvalidateMegaBuffer(); // Since we're writing to the backing buffer the megabuffer contents will require refresh

        if (dirtyState == DirtyState::CpuDirty) {
            SynchronizeHostWithCycle(pCycle); // Perform a CPU -> GPU sync to ensure correct ordering of writes
        } else if (dirtyState == DirtyState::GpuDirty) {
            // If this buffer was attached to the current cycle, flush all pending host GPU work and wait to ensure that writes are correctly ordered
            if (!cycle.owner_before(pCycle))
                flushHostCallback();

            SynchronizeGuest();
        }

        if (dirtyState != DirtyState::Clean)
            Logger::Error("Attempting to write to a dirty buffer"); // This should never happen since we do syncs in both directions above

        std::memcpy(mirror.data() + offset, data.data(), data.size()); // Always copy to mirror since any CPU side reads will need the up-to-date contents

        if (megaBufferingEnabled) {
            // If megabuffering is enabled then we don't need to do any special sequencing here, we can write directly to the backing and the sequencing for it will be handled at usage time
            std::memcpy(backing.data() + offset, data.data(), data.size());
        } else {
            // Fallback to a GPU-side inline update for the buffer contents to ensure correct sequencing with draws
            gpuCopyCallback();
        }
    }

    Buffer::BufferViewStorage::BufferViewStorage(vk::DeviceSize offset, vk::DeviceSize size, vk::Format format) : offset(offset), size(size), format(format) {}

    Buffer::BufferDelegate::BufferDelegate(std::shared_ptr<Buffer> pBuffer, Buffer::BufferViewStorage *view) : buffer(std::move(pBuffer)), view(view) {
        iterator = buffer->delegates.emplace(buffer->delegates.end(), this);
    }

    Buffer::BufferDelegate::~BufferDelegate() {
        std::scoped_lock lock(*this);
        buffer->delegates.erase(iterator);
    }

    void Buffer::BufferDelegate::lock() {
        auto lBuffer{std::atomic_load(&buffer)};
        while (true) {
            lBuffer->lock();

            auto latestBacking{std::atomic_load(&buffer)};
            if (lBuffer == latestBacking)
                return;

            lBuffer->unlock();
            lBuffer = latestBacking;
        }
    }

    void Buffer::BufferDelegate::unlock() {
        buffer->unlock();
    }

    bool Buffer::BufferDelegate::try_lock() {
        auto lBuffer{std::atomic_load(&buffer)};
        while (true) {
            bool success{lBuffer->try_lock()};

            auto latestBuffer{std::atomic_load(&buffer)};
            if (lBuffer == latestBuffer)
                // We want to ensure that the try_lock() was on the latest backing and not on an outdated one
                return success;

            if (success)
                // We only unlock() if the try_lock() was successful and we acquired the mutex
                lBuffer->unlock();
            lBuffer = latestBuffer;
        }
    }

    BufferView Buffer::GetView(vk::DeviceSize offset, vk::DeviceSize size, vk::Format format) {
        for (auto &view : views)
            if (view.offset == offset && view.size == size && view.format == format)
                return BufferView{shared_from_this(), &view};

        views.emplace_back(offset, size, format);
        return BufferView{shared_from_this(), &views.back()};
    }

    vk::DeviceSize Buffer::AcquireMegaBuffer() {
        SynchronizeGuest(false, true); // First try and enable megabuffering by doing an immediate sync

        if (!megaBufferingEnabled)
            return 0; // Bail out if megabuffering is disabled for this buffer

        SynchronizeHost(); // Since pushes to the megabuffer use the GPU backing contents ensure they're up-to-date by performing a CPU -> GPU sync

        if (megaBufferOffset)
            return megaBufferOffset; // If the current buffer contents haven't been changed since the last acquire, we can just return the existing offset

        megaBufferOffset = gpu.buffer.megaBuffer.Push(backing, true); // Buffers are required to be page aligned in the megabuffer
        return megaBufferOffset;
    }

    void Buffer::InvalidateMegaBuffer() {
        megaBufferOffset = 0;
    }

    BufferView::BufferView(std::shared_ptr<Buffer> buffer, Buffer::BufferViewStorage *view) : bufferDelegate(std::make_shared<Buffer::BufferDelegate>(std::move(buffer), view)) {}

    void BufferView::AttachCycle(const std::shared_ptr<FenceCycle> &cycle) {
        auto buffer{bufferDelegate->buffer.get()};
        if (!buffer->cycle.owner_before(cycle)) {
            buffer->WaitOnFence();
            buffer->cycle = cycle;
            cycle->AttachObject(bufferDelegate);
        }
    }

    void BufferView::RegisterUsage(const std::function<void(const Buffer::BufferViewStorage &, const std::shared_ptr<Buffer> &)> &usageCallback) {
        usageCallback(*bufferDelegate->view, bufferDelegate->buffer);
        if (!bufferDelegate->usageCallback) {
            bufferDelegate->usageCallback = usageCallback;
        } else {
            bufferDelegate->usageCallback = [usageCallback, oldCallback = std::move(bufferDelegate->usageCallback)](const Buffer::BufferViewStorage &pView, const std::shared_ptr<Buffer> &buffer) {
                oldCallback(pView, buffer);
                usageCallback(pView, buffer);
            };
        }
    }

    void BufferView::Read(const std::shared_ptr<FenceCycle> &pCycle, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize offset) const {
        bufferDelegate->buffer->Read(pCycle, flushHostCallback, data, offset + bufferDelegate->view->offset);
    }

    void BufferView::Write(const std::shared_ptr<FenceCycle> &pCycle, const std::function<void()> &flushHostCallback, const std::function<void()> &gpuCopyCallback, span<u8> data, vk::DeviceSize offset) const {
        bufferDelegate->buffer->Write(pCycle, flushHostCallback, gpuCopyCallback, data, offset + bufferDelegate->view->offset);
    }

    vk::DeviceSize BufferView::AcquireMegaBuffer() const {
        vk::DeviceSize bufferOffset{bufferDelegate->buffer->AcquireMegaBuffer()};

        // Propagate 0 results since they signify that megabuffering isn't supported for a buffer
        if (bufferOffset)
            return bufferOffset + bufferDelegate->view->offset;
        else
            return 0;
    }
}
