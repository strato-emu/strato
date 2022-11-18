// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <kernel/memory.h>
#include <kernel/types/KProcess.h>
#include <common/trace.h>
#include <common/settings.h>
#include "buffer.h"

namespace skyline::gpu {
    void Buffer::ResetMegabufferState() {
        if (megaBufferTableUsed)
            megaBufferTableValidity.reset();

        megaBufferTableUsed = false;
        megaBufferViewAccumulatedSize = 0;
        unifiedMegaBuffer = {};
    }

    void Buffer::SetupGuestMappings() {
        u8 *alignedData{util::AlignDown(guest->data(), constant::PageSize)};
        size_t alignedSize{static_cast<size_t>(util::AlignUp(guest->data() + guest->size(), constant::PageSize) - alignedData)};

        alignedMirror = gpu.state.process->memory.CreateMirror(span<u8>{alignedData, alignedSize});
        mirror = alignedMirror.subspan(static_cast<size_t>(guest->data() - alignedData), guest->size());

        // We can't just capture this in the lambda since the lambda could exceed the lifetime of the buffer
        std::weak_ptr<Buffer> weakThis{shared_from_this()};
        trapHandle = gpu.state.nce->CreateTrap(*guest, [weakThis] {
            auto buffer{weakThis.lock()};
            if (!buffer)
                return;

            std::unique_lock stateLock{buffer->stateMutex};
            if (buffer->AllCpuBackingWritesBlocked() || buffer->dirtyState == DirtyState::GpuDirty) {
                stateLock.unlock(); // If the lock isn't unlocked, a deadlock from threads waiting on the other lock can occur

                // If this mutex would cause other callbacks to be blocked then we should block on this mutex in advance
                std::shared_ptr<FenceCycle> waitCycle{};
                do {
                    if (waitCycle) {
                        i64 startNs{buffer->accumulatedGuestWaitCounter > FastReadbackHackWaitCountThreshold ? util::GetTimeNs() : 0};
                        waitCycle->Wait();
                        if (startNs)
                            buffer->accumulatedGuestWaitTime += std::chrono::nanoseconds(util::GetTimeNs() - startNs);

                        buffer->accumulatedGuestWaitCounter++;
                    }

                    std::scoped_lock lock{*buffer};
                    if (waitCycle && buffer->cycle == waitCycle) {
                        buffer->cycle = {};
                        waitCycle = {};
                    } else {
                        waitCycle = buffer->cycle;
                    }
                } while (waitCycle);
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

            if (buffer->cycle)
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

            if (buffer->accumulatedGuestWaitTime > FastReadbackHackWaitTimeThreshold && *buffer->gpu.state.settings->enableFastGpuReadbackHack) {
                // As opposed to skipping readback as we do for textures, with buffers we can still perform the readback but just without syncinc the GPU
                // While the read data may be invalid it's still better than nothing and works in most cases
                memcpy(buffer->mirror.data(), buffer->backing.data(), buffer->mirror.size());
                buffer->dirtyState = DirtyState::Clean;
                return true;
            }

            std::unique_lock lock{*buffer, std::try_to_lock};
            if (!lock)
                return false;

            if (buffer->cycle)
                return false;

            buffer->SynchronizeGuest(true); // We need to assume the buffer is dirty since we don't know what the guest is writing
            buffer->dirtyState = DirtyState::CpuDirty;

            return true;
        });
    }

    Buffer::Buffer(LinearAllocatorState<> &delegateAllocator, GPU &gpu, GuestBuffer guest, size_t id)
        : gpu{gpu},
          backing{gpu.memory.AllocateBuffer(guest.size())},
          guest{guest},
          delegate{delegateAllocator.EmplaceUntracked<BufferDelegate>(this)},
          id{id},
          megaBufferTableShift{std::max(std::bit_width(guest.size() / MegaBufferTableMaxEntries - 1), MegaBufferTableShiftMin)} {
        megaBufferTable.resize(guest.size() / (1 << megaBufferTableShift));
    }

    Buffer::Buffer(LinearAllocatorState<> &delegateAllocator, GPU &gpu, vk::DeviceSize size, size_t id)
        : gpu{gpu},
          backing{gpu.memory.AllocateBuffer(size)},
          delegate{delegateAllocator.EmplaceUntracked<BufferDelegate>(this)},
          id{id} {
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

        if (cycle) {
            cycle->Wait();
            cycle = nullptr;
        }
    }

    bool Buffer::PollFence() {
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
        if (dirtyState == DirtyState::GpuDirty)
            SynchronizeGuestImmediate(isFirstUsage, flushHostCallback);

        std::memcpy(data.data(), mirror.data() + offset, data.size());
    }

    bool Buffer::Write(span<u8> data, vk::DeviceSize offset, const std::function<void()> &gpuCopyCallback) {
        AdvanceSequence(); // We are modifying GPU backing contents so advance to the next sequence
        everHadInlineUpdate = true;

        // We cannot have *ANY* state changes for the duration of this function, if the buffer became CPU dirty partway through the GPU writes would mismatch the CPU writes
        std::scoped_lock lock{stateMutex};

        // If the buffer is GPU dirty do the write on the GPU and we're done
        if (dirtyState == DirtyState::GpuDirty) {
            if (gpuCopyCallback)
                gpuCopyCallback();
            else
                return true;
        }

        if (dirtyState == DirtyState::CpuDirty && SequencedCpuBackingWritesBlocked())
            // If the buffer is used in sequence directly on the GPU, SynchronizeHost before modifying the mirror contents to ensure proper sequencing. This write will then be sequenced on the GPU instead (the buffer will be kept clean for the rest of the execution due to gpuCopyCallback blocking all writes)
            SynchronizeHost();

        std::memcpy(mirror.data() + offset, data.data(), data.size()); // Always copy to mirror since any CPU side reads will need the up-to-date contents

        if (dirtyState == DirtyState::CpuDirty && !SequencedCpuBackingWritesBlocked())
            // Skip updating backing if the changes are gonna be updated later by SynchroniseHost in executor anyway
            return false;

        if (!SequencedCpuBackingWritesBlocked() && PollFence()) {
            // We can write directly to the backing as long as this resource isn't being actively used by a past workload (in the current context or another)
            std::memcpy(backing.data() + offset, data.data(), data.size());
        } else {
            // If this buffer is host immutable, perform a GPU-side inline update for the buffer contents since we can't directly modify the backing
            // If no copy callback is supplied, return true to indicate that the caller should repeat the write with an appropriate callback
            if (gpuCopyCallback)
                gpuCopyCallback();
            else
                return true;
        }

        return false;
    }

    void Buffer::CopyFrom(vk::DeviceSize dstOffset, Buffer *src, vk::DeviceSize srcOffset, vk::DeviceSize size, const std::function<void()> &gpuCopyCallback) {
        AdvanceSequence(); // We are modifying GPU backing contents so advance to the next sequence
        everHadInlineUpdate = true;

        std::scoped_lock lock{stateMutex, src->stateMutex}; // Fine even if src and dst are same since recursive mutex

        if (dirtyState == DirtyState::CpuDirty && SequencedCpuBackingWritesBlocked())
            // If the buffer is used in sequence directly on the GPU, SynchronizeHost before modifying the mirror contents to ensure proper sequencing. This write will then be sequenced on the GPU instead (the buffer will be kept clean for the rest of the execution due to gpuCopyCallback blocking all writes)
            SynchronizeHost();

        if (dirtyState != DirtyState::GpuDirty && src->dirtyState != DirtyState::GpuDirty) {
            std::memcpy(mirror.data() + dstOffset, src->mirror.data() + srcOffset, size);

            if (dirtyState == DirtyState::CpuDirty && !SequencedCpuBackingWritesBlocked())
                // Skip updating backing if the changes are gonna be updated later by SynchroniseHost in executor anyway
                return;

            if (!SequencedCpuBackingWritesBlocked() && PollFence()) {
                // We can write directly to the backing as long as this resource isn't being actively used by a past workload (in the current context or another)
                std::memcpy(backing.data() + dstOffset, src->mirror.data() + srcOffset, size);
            } else {
                gpuCopyCallback();
            }
        } else {
            MarkGpuDirty();
            gpuCopyCallback();
        }
    }


    BufferView Buffer::GetView(vk::DeviceSize offset, vk::DeviceSize size) {
        return BufferView{delegate, offset, size};
    }

    BufferView Buffer::TryGetView(span<u8> mapping) {
        if (guest->contains(mapping))
            return GetView(static_cast<vk::DeviceSize>(std::distance(guest->begin(), mapping.begin())), mapping.size());
        else
            return {};
    }

    BufferBinding Buffer::TryMegaBufferView(const std::shared_ptr<FenceCycle> &pCycle, MegaBufferAllocator &allocator, u32 executionNumber,
                                            vk::DeviceSize offset, vk::DeviceSize size) {
        if ((!everHadInlineUpdate && sequenceNumber < FrequentlySyncedThreshold) || size >= MegaBufferChunkSize)
            // Don't megabuffer buffers that have never had inline updates and are not frequently synced since performance is only going to be harmed as a result of the constant copying and there wont be any benefit since there are no GPU inline updates that would be avoided
            return {};

        // We are safe to check dirty state here since it will only ever be set GPU dirty with the buffer locked and from the active GPFIFO thread. This helps with perf since the lock ends up being slightly expensive
        if (dirtyState == DirtyState::GpuDirty)
            // Bail out if buffer cannot be synced, we don't know the contents ahead of time so the sequence is indeterminate
            return {};

        // If the active execution has changed all previous allocations are now invalid
        if (executionNumber != lastExecutionNumber) [[unlikely]] {
            ResetMegabufferState();
            lastExecutionNumber = executionNumber;
        }

        // If more than half the buffer has been megabuffered in chunks within the same execution assume this will generally be the case for this buffer and just megabuffer the whole thing without chunking
        if (unifiedMegaBufferEnabled || (megaBufferViewAccumulatedSize > (backing.size() / 2) && backing.size() < MegaBufferChunkSize)) {
            if (!unifiedMegaBuffer) {
                unifiedMegaBuffer = allocator.Push(pCycle, mirror, true);
                unifiedMegaBufferEnabled = true;
            }

            return BufferBinding{unifiedMegaBuffer.buffer, unifiedMegaBuffer.offset + offset, size};
        }

        if (size > MegaBufferingDisableThreshold) {
            megaBufferViewAccumulatedSize += size;
            return {};
        }

        size_t entryIdx{offset >> megaBufferTableShift};
        size_t bufferEntryOffset{entryIdx << megaBufferTableShift};
        size_t entryViewOffset{offset - bufferEntryOffset};

        if (entryIdx >= megaBufferTable.size())
            return {};

        auto &allocation{megaBufferTable[entryIdx]};

        // If the cached allocation is invalid or too small, allocate a new one
        if (!megaBufferTableValidity.test(entryIdx) || allocation.region.size() < (size + entryViewOffset)) {
            // Use max(oldSize, newSize) to avoid redundant reallocations within an execution if a larger allocation comes along later
            auto mirrorAllocationRegion{mirror.subspan(bufferEntryOffset, std::max(entryViewOffset + size, allocation.region.size()))};
            allocation = allocator.Push(pCycle, mirrorAllocationRegion, true);
            megaBufferTableValidity.set(entryIdx);
            megaBufferViewAccumulatedSize += mirrorAllocationRegion.size();
            megaBufferTableUsed = true;
        }

        return {allocation.buffer, allocation.offset + entryViewOffset, size};
    }

    void Buffer::AdvanceSequence() {
        ResetMegabufferState();
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
        accumulatedCpuLockCounter++;
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
        AllowAllBackingWrites();
        lastExecutionNumber = 0;
        mutex.unlock();
    }

    bool Buffer::try_lock() {
        if (mutex.try_lock()) {
            accumulatedCpuLockCounter++;
            return true;
        }
        return false;
    }

    BufferDelegate::BufferDelegate(Buffer *buffer) : buffer{buffer} {}

    Buffer *BufferDelegate::GetBuffer() {
        if (linked) [[unlikely]]
            return link->GetBuffer();
        else
            return buffer;
    }

    void BufferDelegate::Link(BufferDelegate *newTarget, vk::DeviceSize newOffset) {
        if (linked)
            throw exception("Cannot link a buffer delegate that is already linked!");

        linked = true;
        link = newTarget;
        offset = newOffset;
    }

    vk::DeviceSize BufferDelegate::GetOffset() {
        if (linked) [[unlikely]]
            return link->GetOffset() + offset;
        else
            return offset;
    }

    void BufferView::ResolveDelegate() {
        offset += delegate->GetOffset();
        delegate = delegate->GetBuffer()->delegate;
    }

    BufferView::BufferView() {}

    BufferView::BufferView(BufferDelegate *delegate, vk::DeviceSize offset, vk::DeviceSize size) : delegate{delegate}, offset{offset}, size{size} {}

    Buffer *BufferView::GetBuffer() const {
        return delegate->GetBuffer();
    }

    vk::DeviceSize BufferView::GetOffset() const {
        return offset + delegate->GetOffset();
    }

    void BufferView::Read(bool isFirstUsage, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize readOffset) const {
        GetBuffer()->Read(isFirstUsage, flushHostCallback, data, readOffset + GetOffset());
    }

    bool BufferView::Write(span<u8> data, vk::DeviceSize writeOffset, const std::function<void()> &gpuCopyCallback) const {
        return GetBuffer()->Write(data, writeOffset + GetOffset(), gpuCopyCallback);
    }

    BufferBinding BufferView::TryMegaBuffer(const std::shared_ptr<FenceCycle> &pCycle, MegaBufferAllocator &allocator, u32 executionNumber, size_t sizeOverride) const {
        return GetBuffer()->TryMegaBufferView(pCycle, allocator, executionNumber, GetOffset(), sizeOverride ? sizeOverride : size);
    }

    span<u8> BufferView::GetReadOnlyBackingSpan(bool isFirstUsage, const std::function<void()> &flushHostCallback) {
        auto backing{delegate->GetBuffer()->GetReadOnlyBackingSpan(isFirstUsage, flushHostCallback)};
        return backing.subspan(GetOffset(), size);
    }

    void BufferView::CopyFrom(BufferView src, const std::function<void()> &gpuCopyCallback) {
        if (src.size != size)
            throw exception("Copy size mismatch!");
        return GetBuffer()->CopyFrom(GetOffset(), src.GetBuffer(), src.GetOffset(), size, gpuCopyCallback);
    }

}
