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

        // We can't just capture this in the lambda since the lambda could exceed the lifetime of the buffer
        std::weak_ptr<Buffer> weakThis{shared_from_this()};
        trapHandle = gpu.state.nce->CreateTrap(*guest, [weakThis] {
            auto buffer{weakThis.lock()};
            if (!buffer)
                return;

            std::unique_lock stateLock{buffer->stateMutex};
            if (buffer->AllCpuBackingWritesBlocked()) {
                stateLock.unlock(); // If the lock isn't unlocked, a deadlock from threads waiting on the other lock can occur

                // If this mutex would cause other callbacks to be blocked then we should block on this mutex in advance
                std::scoped_lock lock{*buffer};
            }
        }, [weakThis] {
            TRACE_EVENT("gpu", "Buffer::ReadTrap");

            auto buffer{weakThis.lock()};
            if (!buffer)
                return true;

            std::unique_lock stateLock{buffer->stateMutex, std::try_to_lock};
            if (!stateLock)
                return false;

            if (buffer->dirtyState != DirtyState::GpuDirty)
                return true; // If state is already CPU dirty/Clean we don't need to do anything

            std::unique_lock lock{*buffer, std::try_to_lock};
            if (!lock)
                return false;

            buffer->SynchronizeGuest(true); // We can skip trapping since the caller will do it
            return true;
        }, [weakThis] {
            TRACE_EVENT("gpu", "Buffer::WriteTrap");

            auto buffer{weakThis.lock()};
            if (!buffer)
                return true;

            std::unique_lock stateLock{buffer->stateMutex, std::try_to_lock};
            if (!stateLock)
                return false;

            if (!buffer->AllCpuBackingWritesBlocked() && buffer->dirtyState != DirtyState::GpuDirty) {
                buffer->dirtyState = DirtyState::CpuDirty;
                return true;
            }

            std::unique_lock lock{*buffer, std::try_to_lock};
            if (!lock)
                return false;

            buffer->WaitOnFence();
            buffer->SynchronizeGuest(true); // We need to assume the buffer is dirty since we don't know what the guest is writing
            buffer->dirtyState = DirtyState::CpuDirty;

            return true;
        });
    }

    Buffer::Buffer(GPU &gpu, GuestBuffer guest) : gpu{gpu}, backing{gpu.memory.AllocateBuffer(guest.size())}, guest{guest} {}

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

        std::scoped_lock lock{stateMutex}; // stateMutex is locked to prevent state changes at any point during this function

        if (dirtyState == DirtyState::GpuDirty)
            return;

        gpu.state.nce->TrapRegions(*trapHandle, false); // This has to occur prior to any synchronization as it'll skip trapping

        if (dirtyState == DirtyState::CpuDirty)
            SynchronizeHost(true); // Will transition the Buffer to Clean

        dirtyState = DirtyState::GpuDirty;
        gpu.state.nce->PageOutRegions(*trapHandle); // All data can be paged out from the guest as the guest mirror won't be used

        BlockAllCpuBackingWrites();
        AdvanceSequence(); // The GPU will modify buffer contents so advance to the next sequence
    }

    void Buffer::WaitOnFence() {
        TRACE_EVENT("gpu", "Buffer::WaitOnFence");

        std::scoped_lock lock{stateMutex};

        if (cycle) {
            cycle->Wait();
            cycle = nullptr;
        }
    }

    bool Buffer::PollFence() {
        std::scoped_lock lock{stateMutex};

        if (!cycle)
            return true;

        if (cycle->Poll()) {
            cycle = nullptr;
            return true;
        }

        return false;
    }

    void Buffer::Invalidate() {
        if (trapHandle) {
            gpu.state.nce->DeleteTrap(*trapHandle);
            trapHandle = {};
        }

        // Will prevent any sync operations so even if the trap handler is partway through running and hasn't yet acquired the lock it won't do anything
        guest = {};
    }

    void Buffer::SynchronizeHost(bool skipTrap) {
        if (!guest)
            return;

        TRACE_EVENT("gpu", "Buffer::SynchronizeHost");

        {
            std::scoped_lock lock{stateMutex};
            if (dirtyState != DirtyState::CpuDirty)
                return;

            dirtyState = DirtyState::Clean;
            WaitOnFence();

            AdvanceSequence(); // We are modifying GPU backing contents so advance to the next sequence

            if (!skipTrap)
                gpu.state.nce->TrapRegions(*trapHandle, true); // Trap any future CPU writes to this buffer, must be done before the memcpy so that any modifications during the copy are tracked
        }

        std::memcpy(backing.data(), mirror.data(), mirror.size());
    }

    bool Buffer::SynchronizeGuest(bool skipTrap, bool nonBlocking) {
        if (!guest)
            return false;

        TRACE_EVENT("gpu", "Buffer::SynchronizeGuest");

        {
            std::scoped_lock lock{stateMutex};

            if (dirtyState != DirtyState::GpuDirty)
                return true; // If the buffer is not dirty, there is no need to synchronize it

            if (nonBlocking && !PollFence())
                return false; // If the fence is not signalled and non-blocking behaviour is requested then bail out

            WaitOnFence();
            std::memcpy(mirror.data(), backing.data(), mirror.size());

            dirtyState = DirtyState::Clean;
        }

        if (!skipTrap)
            gpu.state.nce->TrapRegions(*trapHandle, true);

        return true;
    }

    void Buffer::SynchronizeGuestImmediate(bool isFirstUsage, const std::function<void()> &flushHostCallback) {
        // If this buffer was attached to the current cycle, flush all pending host GPU work and wait to ensure that we read valid data
        if (!isFirstUsage)
            flushHostCallback();

        SynchronizeGuest();
    }

    void Buffer::Read(bool isFirstUsage, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize offset) {
        std::scoped_lock lock{stateMutex};
        if (dirtyState == DirtyState::GpuDirty)
            SynchronizeGuestImmediate(isFirstUsage, flushHostCallback);

        std::memcpy(data.data(), mirror.data() + offset, data.size());
    }

    void Buffer::Write(bool isFirstUsage, const std::function<void()> &flushHostCallback, const std::function<void()> &gpuCopyCallback, span<u8> data, vk::DeviceSize offset) {
        AdvanceSequence(); // We are modifying GPU backing contents so advance to the next sequence
        everHadInlineUpdate = true;

        // We cannot have *ANY* state changes for the duration of this function, if the buffer became CPU dirty partway through the GPU writes would mismatch the CPU writes
        std::scoped_lock lock{stateMutex};

        // Syncs in both directions to ensure correct ordering of writes
        if (dirtyState == DirtyState::CpuDirty)
            SynchronizeHost();
        else if (dirtyState == DirtyState::GpuDirty)
            SynchronizeGuestImmediate(isFirstUsage, flushHostCallback);

        std::memcpy(mirror.data() + offset, data.data(), data.size()); // Always copy to mirror since any CPU side reads will need the up-to-date contents

        if (!SequencedCpuBackingWritesBlocked() && PollFence())
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
        if (!SynchronizeGuest(false, true))
            // Bail out if buffer cannot be synced, we don't know the contents ahead of time so the sequence is indeterminate
            return {};

        SynchronizeHost(); // Ensure that the returned mirror is fully up-to-date by performing a CPU -> GPU sync

        return {sequenceNumber, mirror};
    }

    void Buffer::AdvanceSequence() {
        sequenceNumber++;
    }

    span<u8> Buffer::GetReadOnlyBackingSpan(bool isFirstUsage, const std::function<void()> &flushHostCallback) {
        std::scoped_lock lock{stateMutex};
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
        backingImmutability = BackingImmutability::None;
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

    void BufferView::RegisterUsage(LinearAllocatorState<> &allocator, const std::shared_ptr<FenceCycle> &cycle, Buffer::BufferDelegate::UsageCallback usageCallback) {
        if (!bufferDelegate->usageCallbacks)
            bufferDelegate->usageCallbacks = decltype(bufferDelegate->usageCallbacks)::value_type{allocator};

        // Users of RegisterUsage expect the buffer contents to be sequenced as the guest GPU would be, so force any further sequenced writes in the current cycle to occur on the GPU
        bufferDelegate->buffer->BlockSequencedCpuBackingWrites();

        usageCallback(*bufferDelegate->view, bufferDelegate->buffer);
        bufferDelegate->usageCallbacks->emplace_back(std::move(usageCallback));
    }

    void BufferView::Read(bool isFirstUsage, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize offset) const {
        bufferDelegate->buffer->Read(isFirstUsage, flushHostCallback, data, offset + bufferDelegate->view->offset);
    }

    void BufferView::Write(bool isFirstUsage, const std::shared_ptr<FenceCycle> &pCycle, const std::function<void()> &flushHostCallback, const std::function<void()> &gpuCopyCallback, span<u8> data, vk::DeviceSize offset) const {
        // If megabuffering can't be enabled we have to do a GPU-side copy to ensure sequencing
        bool gpuCopy{bufferDelegate->view->size > MegaBufferingDisableThreshold};
        if (gpuCopy)
            bufferDelegate->buffer->BlockSequencedCpuBackingWrites();

        bufferDelegate->buffer->Write(isFirstUsage, flushHostCallback, gpuCopyCallback, data, offset + bufferDelegate->view->offset);
    }

    MegaBufferAllocator::Allocation BufferView::AcquireMegaBuffer(const std::shared_ptr<FenceCycle> &pCycle, MegaBufferAllocator &allocator) const {
        if (!bufferDelegate->buffer->EverHadInlineUpdate())
            // Don't megabuffer buffers that have never had inline updates since performance is only going to be harmed as a result of the constant copying and there wont be any benefit since there are no GPU inline updates that would be avoided
            return {};

        if (bufferDelegate->view->size > MegaBufferingDisableThreshold)
            return {};

        auto [newSequence, sequenceSpan]{bufferDelegate->buffer->AcquireCurrentSequence()};
        if (!newSequence)
            return {}; // If the sequence can't be acquired then the buffer is GPU dirty and we can't megabuffer

        // If a copy of the view for the current sequence is already in megabuffer then we can just use that
        if (newSequence == bufferDelegate->view->lastAcquiredSequence && bufferDelegate->view->megaBufferAllocation)
            return bufferDelegate->view->megaBufferAllocation;

        // If the view is not in the megabuffer then we need to allocate a new copy
        auto viewBackingSpan{sequenceSpan.subspan(bufferDelegate->view->offset, bufferDelegate->view->size)};

        // TODO: we could optimise the alignment requirements here based on buffer usage
        bufferDelegate->view->megaBufferAllocation = allocator.Push(pCycle, viewBackingSpan, true);
        bufferDelegate->view->lastAcquiredSequence = newSequence;

        return bufferDelegate->view->megaBufferAllocation; // Success!
    }

    span<u8> BufferView::GetReadOnlyBackingSpan(bool isFirstUsage, const std::function<void()> &flushHostCallback) {
        auto backing{bufferDelegate->buffer->GetReadOnlyBackingSpan(isFirstUsage, flushHostCallback)};
        return backing.subspan(bufferDelegate->view->offset, bufferDelegate->view->size);
    }
}
