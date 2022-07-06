// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <kernel/memory.h>
#include <kernel/types/KProcess.h>
#include <common/trace.h>
#include "buffer.h"

namespace skyline::gpu {
    void Buffer::SetupGuestMappings() {
        u8 *alignedData{util::AlignDown(guest->data(), PAGE_SIZE)};
        size_t alignedSize{static_cast<size_t>(util::AlignUp(guest->data() + guest->size(), PAGE_SIZE) - alignedData)};

        alignedMirror = gpu.state.process->memory.CreateMirror(span<u8>{alignedData, alignedSize});
        mirror = alignedMirror.subspan(static_cast<size_t>(guest->data() - alignedData), guest->size());

        trapHandle = gpu.state.nce->TrapRegions(*guest, true, [this] {
            std::scoped_lock lock{*this};
        }, [this] {
            std::unique_lock lock{*this, std::try_to_lock};
            if (!lock)
                return false;
            SynchronizeGuest(true); // We can skip trapping since the caller will do it
            return true;
        }, [this] {
            DirtyState expectedState{DirtyState::Clean};
            if (dirtyState.compare_exchange_strong(expectedState, DirtyState::CpuDirty, std::memory_order_relaxed) || expectedState == DirtyState::CpuDirty)
                return true; // If we can transition the buffer to CPU dirty (from Clean) or if it already is CPU dirty then we can just return, we only need to do the lock and corresponding sync if the buffer is GPU dirty

            std::unique_lock lock{*this, std::try_to_lock};
            if (!lock)
                return false;
            SynchronizeGuest(true);
            dirtyState = DirtyState::CpuDirty; // We need to assume the buffer is dirty since we don't know what the guest is writing
            return true;
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
                if (srcBuffer->cycle && cycle != srcBuffer->cycle)
                    if (cycle)
                        cycle->ChainCycle(srcBuffer->cycle);
                    else
                        cycle = srcBuffer->cycle;

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
        if (!guest)
            return;

        auto currentState{dirtyState.load(std::memory_order_relaxed)};
        do {
            if (currentState == DirtyState::GpuDirty)
                return;
        } while (!dirtyState.compare_exchange_strong(currentState, DirtyState::GpuDirty, std::memory_order_relaxed));

        AdvanceSequence(); // The GPU will modify buffer contents so advance to the next sequence
        gpu.state.nce->RetrapRegions(*trapHandle, false);
    }

    void Buffer::WaitOnFence() {
        TRACE_EVENT("gpu", "Buffer::WaitOnFence");

        if (cycle) {
            cycle->Wait();
            cycle = nullptr;
        }
    }

    bool Buffer::PollFence() {
        if (cycle && cycle->Poll()) {
            cycle = nullptr;
            return true;
        }
        return false;
    }

    void Buffer::SynchronizeHost(bool rwTrap) {
        if (!guest)
            return;

        auto currentState{dirtyState.load(std::memory_order_relaxed)};
        do {
            if (currentState != DirtyState::CpuDirty || !guest)
                return; // If the buffer has not been modified on the CPU, there is no need to synchronize it
        } while (!dirtyState.compare_exchange_strong(currentState, rwTrap ? DirtyState::GpuDirty : DirtyState::Clean, std::memory_order_relaxed));

        TRACE_EVENT("gpu", "Buffer::SynchronizeHost");

        AdvanceSequence(); // We are modifying GPU backing contents so advance to the next sequence

        WaitOnFence();

        std::memcpy(backing.data(), mirror.data(), mirror.size());
        gpu.state.nce->RetrapRegions(*trapHandle, !rwTrap); // Trap any future CPU reads (optionally) + writes to this buffer
    }

    void Buffer::SynchronizeGuest(bool skipTrap, bool nonBlocking) {
        if (!guest)
            return;

        auto currentState{dirtyState.load(std::memory_order_relaxed)};
        do {
            if (currentState != DirtyState::GpuDirty)
                return; // If the buffer has not been used on the GPU, there is no need to synchronize it
        } while (!dirtyState.compare_exchange_strong(currentState, DirtyState::Clean, std::memory_order_relaxed));

        if (nonBlocking && !PollFence())
            return;

        TRACE_EVENT("gpu", "Buffer::SynchronizeGuest");

        if (!skipTrap)
            gpu.state.nce->RetrapRegions(*trapHandle, true);

        if (!nonBlocking)
            WaitOnFence();

        std::memcpy(mirror.data(), backing.data(), mirror.size());
    }

    void Buffer::SynchronizeGuestImmediate(bool isFirstUsage, const std::function<void()> &flushHostCallback) {
        // If this buffer was attached to the current cycle, flush all pending host GPU work and wait to ensure that we read valid data
        if (!isFirstUsage)
            flushHostCallback();

        SynchronizeGuest();
    }

    void Buffer::Read(bool isFirstUsage, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize offset) {
        if (dirtyState == DirtyState::GpuDirty)
            SynchronizeGuestImmediate(isFirstUsage, flushHostCallback);

        std::memcpy(data.data(), mirror.data() + offset, data.size());
    }

    void Buffer::Write(bool isFirstUsage, const std::function<void()> &flushHostCallback, const std::function<void()> &gpuCopyCallback, span<u8> data, vk::DeviceSize offset) {
        AdvanceSequence(); // We are modifying GPU backing contents so advance to the next sequence
        everHadInlineUpdate = true;

        // Perform a syncs in both directions to ensure correct ordering of writes
        if (dirtyState == DirtyState::CpuDirty)
            SynchronizeHost();
        else if (dirtyState == DirtyState::GpuDirty)
            SynchronizeGuestImmediate(isFirstUsage, flushHostCallback);

        // It's possible that the guest will arbitrarily modify the buffer contents on the CPU after the syncs and trigger the signal handler which would set the dirty state to CPU dirty, this is acceptable as there is no requirement to make writes visible immediately

        std::memcpy(mirror.data() + offset, data.data(), data.size()); // Always copy to mirror since any CPU side reads will need the up-to-date contents

        if (!usedByContext && PollFence())
            // We can write directly to the backing as long as this resource isn't being actively used by a past workload (in the current context or another)
            std::memcpy(backing.data() + offset, data.data(), data.size());
        else
            // If this buffer is host immutable, perform a GPU-side inline update for the buffer contents since we can't directly modify the backing
            gpuCopyCallback();
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

    span<u8> Buffer::GetReadOnlyBackingSpan(bool isFirstUsage, const std::function<void()> &flushHostCallback) {
        if (dirtyState == DirtyState::GpuDirty)
            SynchronizeGuestImmediate(isFirstUsage, flushHostCallback);

        return mirror;
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
        usedByContext = false;
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
        buffer.Lock();
    }

    bool Buffer::BufferDelegate::LockWithTag(ContextTag pTag) {
        bool result{};
        buffer.Lock([pTag, &result](Buffer *pBuffer) {
            result = pBuffer->LockWithTag(pTag);
        });
        return result;
    }

    void Buffer::BufferDelegate::unlock() {
        buffer->unlock();
    }

    bool Buffer::BufferDelegate::try_lock() {
        return buffer.TryLock();
    }

    BufferView::BufferView(std::shared_ptr<Buffer> buffer, const Buffer::BufferViewStorage *view) : bufferDelegate(std::make_shared<Buffer::BufferDelegate>(std::move(buffer), view)) {}

    void BufferView::RegisterUsage(const std::shared_ptr<FenceCycle> &cycle, const std::function<void(const Buffer::BufferViewStorage &, const std::shared_ptr<Buffer> &)> &usageCallback) {
        // Users of RegisterUsage expect the buffer contents to be sequenced as the guest GPU would be, so force any further writes in the current cycle to occur on the GPU
        bufferDelegate->buffer->MarkGpuUsed();

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

    void BufferView::Read(bool isFirstUsage, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize offset) const {
        bufferDelegate->buffer->Read(isFirstUsage, flushHostCallback, data, offset + bufferDelegate->view->offset);
    }

    void BufferView::Write(bool isFirstUsage, const std::shared_ptr<FenceCycle> &pCycle, const std::function<void()> &flushHostCallback, const std::function<void()> &gpuCopyCallback, span<u8> data, vk::DeviceSize offset) const {
        // If megabuffering can't be enabled we have to do a GPU-side copy to ensure sequencing
        bool gpuCopy{bufferDelegate->view->size > MegaBufferingDisableThreshold};
        if (gpuCopy)
            // This will force the host buffer contents to stay as is for the current cycle, requiring that write operations are instead sequenced on the GPU for the entire buffer
            bufferDelegate->buffer->MarkGpuUsed();

        bufferDelegate->buffer->Write(isFirstUsage, flushHostCallback, gpuCopyCallback, data, offset + bufferDelegate->view->offset);
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

    span<u8> BufferView::GetReadOnlyBackingSpan(bool isFirstUsage, const std::function<void()> &flushHostCallback) {
        auto backing{bufferDelegate->buffer->GetReadOnlyBackingSpan(isFirstUsage, flushHostCallback)};
        return backing.subspan(bufferDelegate->view->offset, bufferDelegate->view->size);
    }
}
