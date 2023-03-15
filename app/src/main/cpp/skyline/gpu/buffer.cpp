// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <adrenotools/driver.h>
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

    void Buffer::SetupStagedTraps() {
        if (isDirect)
            return;

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
                memcpy(buffer->mirror.data(), buffer->backing->data(), buffer->mirror.size());
                buffer->dirtyState = *buffer->gpu.state.settings->enableFastReadbackWrites ? DirtyState::CpuDirty : DirtyState::Clean;
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

    void Buffer::EnableTrackedShadowDirect() {
        if (!directTrackedShadowActive) {
            directTrackedShadow.resize(guest->size());
            directTrackedShadowActive = true;
        }
    }

    span<u8> Buffer::BeginWriteCpuSequencedDirect(size_t offset, size_t size) {
        EnableTrackedShadowDirect();
        directTrackedWrites.Insert({offset, offset + size});
        return {directTrackedShadow.data() + offset, size};
    }

    bool Buffer::RefreshGpuReadsActiveDirect() {
        bool readsActive{SequencedCpuBackingWritesBlocked() || !PollFence()};
        if (!readsActive) {
            if (directTrackedShadowActive) {
                directTrackedShadowActive = false;
                directTrackedShadow.clear();
                directTrackedShadow.shrink_to_fit();
            }
            directTrackedWrites.Clear();
        }
        
        return readsActive;
    }
    
    bool Buffer::RefreshGpuWritesActiveDirect(bool wait, const std::function<void()> &flushHostCallback) {
        if (directGpuWritesActive && (!PollFence() || AllCpuBackingWritesBlocked())) {
            if (wait) {
                if (AllCpuBackingWritesBlocked()) // If we are dirty in the current cycle we'll need to flush
                    flushHostCallback();

                WaitOnFence();

                // No longer dirty
            } else {
                return true;
            }
        }

        directGpuWritesActive = false;
        return false;
    }

    bool Buffer::ValidateMegaBufferViewImplDirect(vk::DeviceSize size) {
        if (!everHadInlineUpdate || size >= MegaBufferChunkSize)
            // Don't megabuffer buffers that have never had inline updates
            return false;

        if (RefreshGpuWritesActiveDirect())
            // If the buffer is currently being written to by the GPU then we can't megabuffer it
            return false;

        if (directTrackedShadowActive)
            // If the mirror contents aren't fully up to date then we can't megabuffer that would ignore any shadow tracked writes
            return false;

        return true;
    }

    bool Buffer::ValidateMegaBufferViewImplStaged(vk::DeviceSize size) {
        if ((!everHadInlineUpdate && sequenceNumber < FrequentlySyncedThreshold) || size >= MegaBufferChunkSize)
            // Don't megabuffer buffers that have never had inline updates and are not frequently synced since performance is only going to be harmed as a result of the constant copying and there wont be any benefit since there are no GPU inline updates that would be avoided
            return false;

        // We are safe to check dirty state here since it will only ever be set GPU dirty with the buffer locked and from the active GPFIFO thread. This helps with perf since the lock ends up being slightly expensive
        if (dirtyState == DirtyState::GpuDirty)
            // Bail out if buffer cannot be synced, we don't know the contents ahead of time so the sequence is indeterminate
            return false;

        return true;
    }

    bool Buffer::ValidateMegaBufferView(vk::DeviceSize size) {
        return isDirect ? ValidateMegaBufferViewImplDirect(size) : ValidateMegaBufferViewImplStaged(size);
    }

    void Buffer::CopyFromImplDirect(vk::DeviceSize dstOffset,
                                    Buffer *src, vk::DeviceSize srcOffset, vk::DeviceSize size,
                                    UsageTracker &usageTracker, const std::function<void()> &gpuCopyCallback) {
        everHadInlineUpdate = true;
        bool needsGpuTracking{src->RefreshGpuWritesActiveDirect() || RefreshGpuWritesActiveDirect()};
        bool needsCpuTracking{RefreshGpuReadsActiveDirect() && !needsGpuTracking};
        if (needsGpuTracking || needsCpuTracking) {
            if (needsGpuTracking) // Force buffer to be dirty for this cycle if either of the sources are dirty, this is needed as otherwise it could have just been dirty from the previous cycle
                MarkGpuDirty(usageTracker);
            gpuCopyCallback();

            if (needsCpuTracking)
                src->Read(false, {}, BeginWriteCpuSequencedDirect(dstOffset, size), srcOffset);
        } else {
            src->Read(false, {}, {mirror.data() + dstOffset, size}, srcOffset);
        }
    }

    void Buffer::CopyFromImplStaged(vk::DeviceSize dstOffset,
                                    Buffer *src, vk::DeviceSize srcOffset, vk::DeviceSize size,
                                    UsageTracker &usageTracker, const std::function<void()> &gpuCopyCallback) {
        std::scoped_lock lock{stateMutex, src->stateMutex}; // Fine even if src and dst are same since recursive mutex

        if (dirtyState == DirtyState::CpuDirty && SequencedCpuBackingWritesBlocked())
            // If the buffer is used in sequence directly on the GPU, SynchronizeHost before modifying the mirror contents to ensure proper sequencing. This write will then be sequenced on the GPU instead (the buffer will be kept clean for the rest of the execution due to gpuCopyCallback blocking all writes)
            SynchronizeHost();

        if (dirtyState != DirtyState::GpuDirty && src->dirtyState != DirtyState::GpuDirty) {
            std::memcpy(mirror.data() + dstOffset, src->mirror.data() + srcOffset, size);

            if (dirtyState == DirtyState::CpuDirty && !SequencedCpuBackingWritesBlocked())
                // Skip updating backing if the changes are gonna be updated later by SynchroniseHost in executor anyway
                return;

            if (!SequencedCpuBackingWritesBlocked() && PollFence())
                // We can write directly to the backing as long as this resource isn't being actively used by a past workload (in the current context or another)
                std::memcpy(backing->data() + dstOffset, src->mirror.data() + srcOffset, size);
            else
                gpuCopyCallback();
        } else {
            MarkGpuDirty(usageTracker);
            gpuCopyCallback();
        }
    }

    bool Buffer::WriteImplDirect(span<u8> data, vk::DeviceSize offset,
                                 UsageTracker &usageTracker, const std::function<void()> &gpuCopyCallback) {
        // If the buffer is GPU dirty do the write on the GPU and we're done
        if (RefreshGpuWritesActiveDirect()) {
            if (gpuCopyCallback) {
                // Propagate dirtiness to the current cycle, since if this is only dirty in a previous cycle that could change at any time and we would need to have the write saved somewhere for CPU reads
                // By propagating the dirtiness to the current cycle we can avoid this and force a wait on any reads
                MarkGpuDirty(usageTracker);
                gpuCopyCallback();
                return false;
            } else {
                return true;
            }
        }

        if (RefreshGpuReadsActiveDirect()) {
            // If the GPU could read the buffer we need to track the write in the shadow and do the actual write on the GPU
            if (gpuCopyCallback)
                gpuCopyCallback();
            else
                return true;

            BeginWriteCpuSequencedDirect(offset, data.size()).copy_from(data);
            return false;
        }

        // If the GPU isn't accessing the mirror we can just write directly to it
        std::memcpy(mirror.data() + offset, data.data(), data.size());
        return false;
    }

    bool Buffer::WriteImplStaged(span<u8> data, vk::DeviceSize offset, const std::function<void()> &gpuCopyCallback) {
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
            std::memcpy(backing->data() + offset, data.data(), data.size());
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

    void Buffer::ReadImplDirect(const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize offset) {
        // If GPU writes are active then wait until that's no longer the case
        RefreshGpuWritesActiveDirect(true, flushHostCallback);

        if (directTrackedShadowActive && RefreshGpuReadsActiveDirect()) {
            size_t dstOffset{};
            while (dstOffset != data.size()) {
                auto srcOffset{dstOffset + offset};
                auto dstRemaining{data.size() - dstOffset};
                auto result{directTrackedWrites.Query(srcOffset)};
                auto size{result.size ? std::min(result.size, dstRemaining) : dstRemaining};
                auto srcData{result.enclosed ? directTrackedShadow.data() : mirror.data()};
                std::memcpy(data.data() + dstOffset, srcData + srcOffset, size);
                dstOffset += size;
            }
        } else [[likely]] {
            std::memcpy(data.data(), mirror.data() + offset, data.size());
        }
    }

    void Buffer::ReadImplStaged(bool isFirstUsage, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize offset) {
        if (dirtyState == DirtyState::GpuDirty)
            SynchronizeGuestImmediate(isFirstUsage, flushHostCallback);

        std::memcpy(data.data(), mirror.data() + offset, data.size());
    }

    void Buffer::MarkGpuDirtyImplDirect() {
        directGpuWritesActive = true;
        BlockAllCpuBackingWrites();
        AdvanceSequence();
    }

    void Buffer::MarkGpuDirtyImplStaged() {
        std::scoped_lock lock{stateMutex}; // stateMutex is locked to prevent state changes at any point during this function

        if (dirtyState == DirtyState::GpuDirty)
            return;

        gpu.state.nce->TrapRegions(*trapHandle, false); // This has to occur prior to any synchronization as it'll skip trapping

        if (dirtyState == DirtyState::CpuDirty)
            SynchronizeHost(true); // Will transition the Buffer to Clean

        dirtyState = DirtyState::GpuDirty;

        BlockAllCpuBackingWrites();
        AdvanceSequence(); // The GPU will modify buffer contents so advance to the next sequence
    }

    void Buffer::MarkGpuDirtyImpl() {
        currentExecutionGpuDirty = true;

        if (isDirect)
            MarkGpuDirtyImplDirect();
        else
            MarkGpuDirtyImplStaged();
    }

    Buffer::Buffer(LinearAllocatorState<> &delegateAllocator, GPU &gpu, GuestBuffer guest, size_t id, bool direct)
        : gpu{gpu},
          guest{guest},
          mirror{gpu.state.process->memory.CreateMirror(guest)},
          delegate{delegateAllocator.EmplaceUntracked<BufferDelegate>(this)},
          isDirect{direct},
          id{id},
          megaBufferTableShift{std::max(std::bit_width(guest.size() / MegaBufferTableMaxEntries - 1), MegaBufferTableShiftMin)} {
        if (isDirect)
            directBacking = gpu.memory.ImportBuffer(mirror);
        else
            backing = gpu.memory.AllocateBuffer(mirror.size());

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
        if (mirror.valid())
            munmap(mirror.data(), mirror.size());
        WaitOnFence();
    }

    void Buffer::MarkGpuDirty(UsageTracker &usageTracker) {
        if (!guest)
            return;

        usageTracker.dirtyIntervals.Insert(*guest);
        MarkGpuDirtyImpl();
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
        if (!guest || isDirect)
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

        std::memcpy(backing->data(), mirror.data(), mirror.size());
    }

    bool Buffer::SynchronizeGuest(bool skipTrap, bool nonBlocking) {
        if (!guest || isDirect)
            return false;

        TRACE_EVENT("gpu", "Buffer::SynchronizeGuest");

        {
            std::scoped_lock lock{stateMutex};

            if (dirtyState != DirtyState::GpuDirty)
                return true; // If the buffer is not dirty, there is no need to synchronize it

            if (nonBlocking && !PollFence())
                return false; // If the fence is not signalled and non-blocking behaviour is requested then bail out

            WaitOnFence();
            std::memcpy(mirror.data(), backing->data(), mirror.size());

            dirtyState = DirtyState::Clean;
        }

        if (!skipTrap)
            gpu.state.nce->TrapRegions(*trapHandle, true);

        return true;
    }

    void Buffer::SynchronizeGuestImmediate(bool isFirstUsage, const std::function<void()> &flushHostCallback) {
        if (isDirect)
            return;

        // If this buffer was attached to the current cycle, flush all pending host GPU work and wait to ensure that we read valid data
        if (!isFirstUsage)
            flushHostCallback();

        SynchronizeGuest();
    }

    void Buffer::Read(bool isFirstUsage, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize offset) {
        if (isDirect)
            ReadImplDirect(flushHostCallback, data, offset);
        else
            ReadImplStaged(isFirstUsage, flushHostCallback, data, offset);
    }

    bool Buffer::Write(span<u8> data, vk::DeviceSize offset, UsageTracker &usageTracker, const std::function<void()> &gpuCopyCallback) {
        AdvanceSequence(); // We are modifying GPU backing contents so advance to the next sequence
        everHadInlineUpdate = true;

        usageTracker.sequencedIntervals.Insert(*guest);

        if (isDirect)
            return WriteImplDirect(data, offset, usageTracker, gpuCopyCallback);
        else
            return WriteImplStaged(data, offset, gpuCopyCallback);
    }

    void Buffer::CopyFrom(vk::DeviceSize dstOffset,
                          Buffer *src, vk::DeviceSize srcOffset, vk::DeviceSize size,
                          UsageTracker &usageTracker, const std::function<void()> &gpuCopyCallback) {
        AdvanceSequence(); // We are modifying GPU backing contents so advance to the next sequence
        everHadInlineUpdate = true;

        usageTracker.sequencedIntervals.Insert(*guest);

        if (isDirect)
            CopyFromImplDirect(dstOffset, src, srcOffset, size, usageTracker, gpuCopyCallback);
        else
            CopyFromImplStaged(dstOffset, src, srcOffset, size, usageTracker, gpuCopyCallback);
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

    BufferBinding Buffer::TryMegaBufferView(const std::shared_ptr<FenceCycle> &pCycle, MegaBufferAllocator &allocator, ContextTag executionTag,
                                            vk::DeviceSize offset, vk::DeviceSize size) {
        if (!ValidateMegaBufferView(size))
            return {};

        // If the active execution has changed all previous allocations are now invalid
        if (executionTag != lastExecutionTag) [[unlikely]] {
            ResetMegabufferState();
            lastExecutionTag = executionTag;
        }

        // If more than half the buffer has been megabuffered in chunks within the same execution assume this will generally be the case for this buffer and just megabuffer the whole thing without chunking
        if (unifiedMegaBufferEnabled || (megaBufferViewAccumulatedSize > (mirror.size() / 2) && mirror.size() < MegaBufferChunkSize)) {
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
        if (!isDirect) {
            std::unique_lock lock{stateMutex};
            if (dirtyState == DirtyState::GpuDirty)
                SynchronizeGuestImmediate(isFirstUsage, flushHostCallback);
        } else {
            RefreshGpuWritesActiveDirect(true, flushHostCallback);
        }

        return mirror;
    }

    void Buffer::PopulateReadBarrier(vk::PipelineStageFlagBits dstStage, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask) {
        if (currentExecutionGpuDirty) {
            srcStageMask |= vk::PipelineStageFlagBits::eAllCommands;
            dstStageMask |= dstStage;
        }
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
        currentExecutionGpuDirty = false;
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

    BufferBinding BufferView::GetBinding(GPU &gpu) const {
        std::scoped_lock lock{gpu.buffer.recreationMutex};
        return {delegate->GetBuffer()->GetBacking(), offset + delegate->GetOffset(), size};
    }

    vk::DeviceSize BufferView::GetOffset() const {
        return offset + delegate->GetOffset();
    }

    void BufferView::Read(bool isFirstUsage, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize readOffset) const {
        GetBuffer()->Read(isFirstUsage, flushHostCallback, data, readOffset + GetOffset());
    }

    bool BufferView::Write(span<u8> data, vk::DeviceSize writeOffset, UsageTracker &usageTracker, const std::function<void()> &gpuCopyCallback) const {
        return GetBuffer()->Write(data, writeOffset + GetOffset(), usageTracker, gpuCopyCallback);
    }

    BufferBinding BufferView::TryMegaBuffer(const std::shared_ptr<FenceCycle> &pCycle, MegaBufferAllocator &allocator, ContextTag executionTag, size_t sizeOverride) const {
        return GetBuffer()->TryMegaBufferView(pCycle, allocator, executionTag, GetOffset(), sizeOverride ? sizeOverride : size);
    }

    span<u8> BufferView::GetReadOnlyBackingSpan(bool isFirstUsage, const std::function<void()> &flushHostCallback) {
        auto backing{delegate->GetBuffer()->GetReadOnlyBackingSpan(isFirstUsage, flushHostCallback)};
        return backing.subspan(GetOffset(), size);
    }

    void BufferView::CopyFrom(BufferView src, UsageTracker &usageTracker, const std::function<void()> &gpuCopyCallback) {
        if (src.size != size)
            throw exception("Copy size mismatch!");
        return GetBuffer()->CopyFrom(GetOffset(), src.GetBuffer(), src.GetOffset(), size, usageTracker, gpuCopyCallback);
    }
}
