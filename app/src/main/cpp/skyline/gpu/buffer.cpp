// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <kernel/memory.h>
#include <kernel/types/KProcess.h>
#include <common/trace.h>
#include "buffer.h"

namespace skyline::gpu {
    bool Buffer::CheckHostImmutable() {
        if (hostImmutableCycle && hostImmutableCycle->Poll())
            hostImmutableCycle.reset();

        return hostImmutableCycle != nullptr;
    }

    void Buffer::SetupGuestMappings() {
        u8 *alignedData{util::AlignDown(guest->data(), PAGE_SIZE)};
        size_t alignedSize{static_cast<size_t>(util::AlignUp(guest->data() + guest->size(), PAGE_SIZE) - alignedData)};

        alignedMirror = gpu.state.process->memory.CreateMirror(span<u8>{alignedData, alignedSize});
        mirror = alignedMirror.subspan(static_cast<size_t>(guest->data() - alignedData), guest->size());

        trapHandle = gpu.state.nce->TrapRegions(*guest, true, [this] {
            std::scoped_lock lock{*this};
            SynchronizeGuest(true); // We can skip trapping since the caller will do it
            WaitOnFence();
        }, [this] {
            std::scoped_lock lock{*this};
            SynchronizeGuest(true);
            dirtyState = DirtyState::CpuDirty; // We need to assume the buffer is dirty since we don't know what the guest is writing
            WaitOnFence();
        });
    }

    Buffer::Buffer(GPU &gpu, GuestBuffer guest) : gpu{gpu}, backing{gpu.memory.AllocateBuffer(guest.size())}, guest{guest} {
        SetupGuestMappings();
    }

    Buffer::Buffer(GPU &gpu, GuestBuffer guest, ContextTag tag, span<std::shared_ptr<Buffer>> srcBuffers) : gpu{gpu}, backing{gpu.memory.AllocateBuffer(guest.size())}, guest{guest} {
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
            ContextLock lock{tag, *srcBuffer};
            if (srcBuffer->guest) {
                if (srcBuffer->hostImmutableCycle) {
                    // Propagate any host immutability
                    if (hostImmutableCycle) {
                        if (srcBuffer->hostImmutableCycle.owner_before(hostImmutableCycle))
                            hostImmutableCycle = srcBuffer->hostImmutableCycle;
                    } else {
                        hostImmutableCycle = srcBuffer->hostImmutableCycle;
                    }
                }

                if (srcBuffer->dirtyState == Buffer::DirtyState::GpuDirty) {
                    // If the source buffer is GPU dirty we cannot directly copy over its GPU backing contents

                    // Only sync back the buffer if it's not attached to the current context, otherwise propagate the GPU dirtiness
                    if (lock.isFirst) {
                        // Perform a GPU -> CPU sync on the source then do a CPU -> GPU sync for the region occupied by the source
                        // This is required since if we were created from a two buffers: one GPU dirty in the current cycle, and one GPU dirty in the previous cycle, if we marked ourselves as CPU dirty here then the GPU dirtiness from the current cycle buffer would be ignored and cause writes to be missed
                        srcBuffer->SynchronizeGuest(true);
                        copyBuffer(guest, *srcBuffer->guest, backing.data(), srcBuffer->mirror.data());
                    } else {
                        MarkGpuDirty();
                    }
                } else if (srcBuffer->dirtyState == Buffer::DirtyState::Clean) {
                    // For clean buffers we can just copy over the GPU backing data directly
                    // This is necessary since clean buffers may not have matching GPU/CPU data in the case of inline updates for host immutable buffers
                    copyBuffer(guest, *srcBuffer->guest, backing.data(), srcBuffer->backing.data());
                }

                // CPU dirty buffers are already synchronized in the initial SynchronizeHost call so don't need special handling
            }
        }
    }

    Buffer::Buffer(GPU &gpu, vk::DeviceSize size) : gpu(gpu), backing(gpu.memory.AllocateBuffer(size)) {
        dirtyState = DirtyState::Clean; // Since this is a host-only buffer it's always going to be clean
    }

    Buffer::~Buffer() {
        if (trapHandle)
            gpu.state.nce->DeleteTrap(*trapHandle);
        SynchronizeGuest(true);
        if (alignedMirror.valid())
            munmap(alignedMirror.data(), alignedMirror.size());
        WaitOnFence();
    }

    void Buffer::MarkGpuDirty() {
        if (dirtyState == DirtyState::GpuDirty || !guest)
            return;

        AdvanceSequence(); // The GPU will modify buffer contents so advance to the next sequence
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

        AdvanceSequence(); // We are modifying GPU backing contents so advance to the next sequence
        std::memcpy(backing.data(), mirror.data(), mirror.size());

        if (rwTrap) {
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

        AdvanceSequence(); // We are modifying GPU backing contents so advance to the next sequence
        std::memcpy(backing.data(), mirror.data(), mirror.size());

        if (rwTrap) {
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

    void Buffer::SynchronizeGuestImmediate(const std::shared_ptr<FenceCycle> &pCycle, const std::function<void()> &flushHostCallback) {
        // If this buffer was attached to the current cycle, flush all pending host GPU work and wait to ensure that we read valid data
        if (cycle.owner_before(pCycle))
            flushHostCallback();

        SynchronizeGuest();
    }

    void Buffer::Read(const std::shared_ptr<FenceCycle> &pCycle, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize offset) {
        if (dirtyState == DirtyState::GpuDirty)
            SynchronizeGuestImmediate(pCycle, flushHostCallback);

        std::memcpy(data.data(), mirror.data() + offset, data.size());
    }

    void Buffer::Write(const std::shared_ptr<FenceCycle> &pCycle, const std::function<void()> &flushHostCallback, const std::function<void()> &gpuCopyCallback, span<u8> data, vk::DeviceSize offset) {
        AdvanceSequence(); // We are modifying GPU backing contents so advance to the next sequence
        everHadInlineUpdate = true;

        // Perform a syncs in both directions to ensure correct ordering of writes
        if (dirtyState == DirtyState::CpuDirty)
            SynchronizeHostWithCycle(pCycle);
        else if (dirtyState == DirtyState::GpuDirty)
            SynchronizeGuestImmediate(pCycle, flushHostCallback);

        if (dirtyState != DirtyState::Clean)
            Logger::Error("Attempting to write to a dirty buffer"); // This should never happen since we do syncs in both directions above

        std::memcpy(mirror.data() + offset, data.data(), data.size()); // Always copy to mirror since any CPU side reads will need the up-to-date contents

        if (CheckHostImmutable())
            // Perform a GPU-side inline update for the buffer contents if this buffer is host immutable since we can't directly modify the backing
            gpuCopyCallback();
        else
            // If that's not the case we don't need to do any GPU-side sequencing here, we can write directly to the backing and the sequencing for it will be handled at usage time
            std::memcpy(backing.data() + offset, data.data(), data.size());
    }

    BufferView Buffer::GetView(vk::DeviceSize offset, vk::DeviceSize size, vk::Format format) {
        // Will return an iterator to the inserted view or the already-existing view if the same view is already in the set
        auto it{views.emplace(offset, size, format).first};
        return BufferView{shared_from_this(), &(*it)};
    }

    std::pair<u64, span<u8>> Buffer::AcquireCurrentSequence() {
        SynchronizeGuest(false, true); // First try to remove GPU dirtiness by doing an immediate sync and taking a quick shower

        if (dirtyState == DirtyState::GpuDirty)
            // Bail out if buffer is GPU dirty - since we don't know the contents ahead of time the sequence is indeterminate
            return {};

        SynchronizeHost(); // Ensure that the returned mirror is fully up-to-date by performing a CPU -> GPU sync

        return {sequenceNumber, mirror};
    }

    void Buffer::AdvanceSequence() {
        sequenceNumber++;
    }

    span<u8> Buffer::GetReadOnlyBackingSpan(const std::shared_ptr<FenceCycle> &pCycle, const std::function<void()> &flushHostCallback) {
        if (dirtyState == DirtyState::GpuDirty)
            SynchronizeGuestImmediate(pCycle, flushHostCallback);

        return mirror;
    }

    void Buffer::MarkHostImmutable(const std::shared_ptr<FenceCycle> &pCycle) {
        hostImmutableCycle = pCycle;
    }

    void Buffer::lock() {
        mutex.lock();
    }

    bool Buffer::LockWithTag(ContextTag pTag) {
        if (pTag && pTag == tag)
            return false;

        mutex.lock();
        tag = pTag;
        return true;
    }

    void Buffer::unlock() {
        tag = ContextTag{};
        mutex.unlock();
    }

    bool Buffer::try_lock() {
        return mutex.try_lock();
    }

    Buffer::BufferViewStorage::BufferViewStorage(vk::DeviceSize offset, vk::DeviceSize size, vk::Format format) : offset(offset), size(size), format(format) {}

    Buffer::BufferDelegate::BufferDelegate(std::shared_ptr<Buffer> pBuffer, const Buffer::BufferViewStorage *view) : buffer(std::move(pBuffer)), view(view) {
        iterator = buffer->delegates.emplace(buffer->delegates.end(), this);
    }

    Buffer::BufferDelegate::~BufferDelegate() {
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

    bool Buffer::BufferDelegate::LockWithTag(ContextTag pTag) {
        auto lBuffer{std::atomic_load(&buffer)};
        while (true) {
            bool didLock{lBuffer->LockWithTag(pTag)};

            auto latestBacking{std::atomic_load(&buffer)};
            if (lBuffer == latestBacking)
                return didLock;

            if (didLock)
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

    BufferView::BufferView(std::shared_ptr<Buffer> buffer, const Buffer::BufferViewStorage *view) : bufferDelegate(std::make_shared<Buffer::BufferDelegate>(std::move(buffer), view)) {}

    void BufferView::AttachCycle(const std::shared_ptr<FenceCycle> &cycle) {
        auto buffer{bufferDelegate->buffer.get()};
        if (!buffer->cycle.owner_before(cycle)) {
            buffer->WaitOnFence();
            buffer->cycle = cycle;
            cycle->AttachObject(bufferDelegate);
        }
    }

    void BufferView::RegisterUsage(const std::shared_ptr<FenceCycle> &pCycle, const std::function<void(const Buffer::BufferViewStorage &, const std::shared_ptr<Buffer> &)> &usageCallback) {
        // Users of RegisterUsage expect the buffer contents to be sequenced as the guest GPU would be, so force any further writes in the current cycle to occur on the GPU
        bufferDelegate->buffer->MarkHostImmutable(pCycle);

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
        // If megabuffering can't be enabled we have to do a GPU-side copy to ensure sequencing
        bool gpuCopy{bufferDelegate->view->size > MegaBufferingDisableThreshold};
        if (gpuCopy)
            // This will force the host buffer contents to stay as is for the current cycle, requiring that write operations are instead sequenced on the GPU for the entire buffer
            bufferDelegate->buffer->MarkHostImmutable(pCycle);

        bufferDelegate->buffer->Write(pCycle, flushHostCallback, gpuCopyCallback, data, offset + bufferDelegate->view->offset);
    }

    vk::DeviceSize BufferView::AcquireMegaBuffer(MegaBuffer &megaBuffer) const {
        if (!bufferDelegate->buffer->EverHadInlineUpdate())
            // Don't megabuffer buffers that have never had inline updates since performance is only going to be harmed as a result of the constant copying and there wont be any benefit since there are no GPU inline updates that would be avoided
            return 0;

        if (bufferDelegate->view->size > MegaBufferingDisableThreshold)
            return 0;

        auto[newSequence, sequenceSpan]{bufferDelegate->buffer->AcquireCurrentSequence()};
        if (!newSequence)
            return 0; // If the sequence can't be acquired then the buffer is GPU dirty and we can't megabuffer

        // If a copy of the view for the current sequence is already in megabuffer then we can just use that
        if (newSequence == bufferDelegate->view->lastAcquiredSequence && bufferDelegate->view->megabufferOffset)
            return bufferDelegate->view->megabufferOffset;

        // If the view is not in the megabuffer then we need to allocate a new copy
        auto viewBackingSpan{sequenceSpan.subspan(bufferDelegate->view->offset, bufferDelegate->view->size)};

        // TODO: we could optimise the alignment requirements here based on buffer usage
        bufferDelegate->view->megabufferOffset = megaBuffer.Push(viewBackingSpan, true);
        bufferDelegate->view->lastAcquiredSequence = newSequence;

        return bufferDelegate->view->megabufferOffset; // Success!
    }

    span<u8> BufferView::GetReadOnlyBackingSpan(const std::shared_ptr<FenceCycle> &pCycle, const std::function<void()> &flushHostCallback) {
        auto backing{bufferDelegate->buffer->GetReadOnlyBackingSpan(pCycle, flushHostCallback)};
        return backing.subspan(bufferDelegate->view->offset, bufferDelegate->view->size);
    }
}
