// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <boost/functional/hash.hpp>
#include <common/linear_allocator.h>
#include <common/spin_lock.h>
#include <nce.h>
#include <gpu/tag_allocator.h>
#include "usage_tracker.h"
#include "megabuffer.h"
#include "memory_manager.h"

namespace skyline::gpu {
    using GuestBuffer = span<u8>; //!< The CPU mapping for the guest buffer, multiple mappings for buffers aren't supported since overlaps cannot be reconciled

    class BufferView;
    class BufferManager;
    class BufferDelegate;

    /**
     * @brief Represents a bound Vulkan buffer that can be used for state updates
     */
    struct BufferBinding {
        vk::Buffer buffer{};
        vk::DeviceSize offset{};
        vk::DeviceSize size{};

        BufferBinding() = default;

        BufferBinding(vk::Buffer buffer, vk::DeviceSize offset = 0, vk::DeviceSize size = 0) : buffer{buffer}, offset{offset}, size{size} {}

        BufferBinding(MegaBufferAllocator::Allocation allocation) : buffer{allocation.buffer}, offset{allocation.offset}, size{allocation.region.size()} {}

        operator bool() const {
            return buffer;
        }
    };

    /**
     * @brief A buffer which is backed by host constructs while being synchronized with the underlying guest buffer
     * @note This class conforms to the Lockable and BasicLockable C++ named requirements
     */
    class Buffer : public std::enable_shared_from_this<Buffer> {
      private:
        GPU &gpu;
        RecursiveSpinLock mutex; //!< Synchronizes any mutations to the buffer or its backing
        std::atomic<ContextTag> tag{}; //!< The tag associated with the last lock call
        std::optional<GuestBuffer> guest;
        std::shared_ptr<FenceCycle> cycle{}; //!< A fence cycle for when any host operation mutating the buffer has completed, it must be waited on prior to any mutations to the backing
        size_t id;
        bool isDirect{}; //!< Indicates if a buffer is directly mapped from the guest

        IntervalList<size_t> directTrackedWrites; //!< (Direct) A list of tracking intervals for the buffer, this is used to determine when to read from `directTrackedShadow`
        std::vector<u8> directTrackedShadow; //!< (Direct) Temporary mirror used to track any CPU-side writes to the buffer while it's being read by the GPU
        bool directTrackedShadowActive{}; //!< (Direct) If `directTrackedShadow` is currently being used to track writes

        span<u8> mirror{}; //!< A contiguous mirror of all the guest mappings to allow linear access on the CPU
        std::optional<memory::Buffer> backing;
        std::optional<memory::ImportedBuffer> directBacking;

        std::optional<nce::NCE::TrapHandle> trapHandle{}; //!< (Staged) The handle of the traps for the guest mappings

        enum class DirtyState {
            Clean, //!< The CPU mappings are in sync with the GPU buffer
            CpuDirty, //!< The CPU mappings have been modified but the GPU buffer is not up to date
            GpuDirty, //!< The GPU buffer has been modified but the CPU mappings have not been updated
        } dirtyState{DirtyState::CpuDirty}; //!< (Staged) The state of the CPU mappings with respect to the GPU buffer
        bool directGpuWritesActive{}; //!< (Direct) If the current/next GPU exection is writing to the buffer (basically GPU dirty)

        enum class BackingImmutability {
            None, //!< Backing can be freely written to and read from
            SequencedWrites, //!< Sequenced writes must not modify the backing on the CPU due to it being read directly on the GPU, but non-sequenced writes can freely occur (SynchroniseHost etc)
            AllWrites //!< No CPU writes to the backing can be performed, all must be sequenced on the GPU or delayed till this is no longer the case
        } backingImmutability{}; //!< Describes how the buffer backing should be accessed by the current context
        RecursiveSpinLock stateMutex; //!< Synchronizes access to the dirty state and backing immutability

        bool currentExecutionGpuDirty{}; //!< If the buffer is GPU dirty within the current execution

        static constexpr u32 InitialSequenceNumber{1}; //!< Sequence number that all buffers start off with
        static constexpr u32 FrequentlySyncedThreshold{6}; //!< Threshold for the sequence number after which the buffer is considered elegible for megabuffering
        u32 sequenceNumber{InitialSequenceNumber}; //!< Sequence number that is incremented after all modifications to the host side `backing` buffer, used to prevent redundant copies of the buffer being stored in the megabuffer by views

        constexpr static vk::DeviceSize MegaBufferingDisableThreshold{1024 * 256}; //!< The threshold at which a view is considered to be too large to be megabuffered (256KiB)

        static constexpr int MegaBufferTableShiftMin{std::countr_zero(0x100U)}; //!< The minimum shift for megabuffer table entries, giving an alignment of at least 256 bytes
        static constexpr size_t MegaBufferTableMaxEntries{0x500U}; //!< Maximum number of entries in the megabuffer table, `megaBufferTableShift` is set based on this and the total buffer size
        int megaBufferTableShift; //!< Shift to apply to buffer offsets to get their megabuffer table index
        std::vector<MegaBufferAllocator::Allocation> megaBufferTable; //!< Table of megabuffer allocations for regions of the buffer
        std::bitset<MegaBufferTableMaxEntries> megaBufferTableValidity{}; //!< Bitset keeping track of which entries in the megabuffer table are valid
        bool megaBufferTableUsed{}; //!< If the megabuffer table has been used at all since the last time it was cleared
        bool unifiedMegaBufferEnabled{}; //!< If the unified megabuffer is enabled for this buffer and should be used instead of the table
        bool everHadInlineUpdate{}; //!< Whether the buffer has ever had an inline update since it was created, if this is set then megabuffering will be attempted by views to avoid the cost of inline GPU updates

        ContextTag lastExecutionTag{}; //!< The execution tag of the last time megabuffer data was updated

        size_t megaBufferViewAccumulatedSize{};
        MegaBufferAllocator::Allocation unifiedMegaBuffer{}; //!< An optional full-size mirror of the buffer in the megabuffer for use when the buffer is frequently updated and *all* of the buffer is frequently used. Replaces all uses of the table when active

        static constexpr size_t FrequentlyLockedThreshold{2}; //!< (Staged) Threshold for the number of times a buffer can be locked (not from context locks, only normal) before it should be considered frequently locked
        size_t accumulatedCpuLockCounter{}; //!< (Staged) Number of times buffer has been locked through non-ContextLocks

        static constexpr size_t FastReadbackHackWaitCountThreshold{6}; //!< (Staged) Threshold for the number of times a buffer can be waited on before it should be considered for the readback hack
        static constexpr std::chrono::nanoseconds FastReadbackHackWaitTimeThreshold{constant::NsInSecond / 4}; //!< (Staged) Threshold for the amount of time buffer texture can be waited on before it should be considered for the readback hack, `SkipReadbackHackWaitCountThreshold` needs to be hit before this
        size_t accumulatedGuestWaitCounter{}; //!< (Staged) Total number of times the buffer has been waited on
        std::chrono::nanoseconds accumulatedGuestWaitTime{}; //!< (Staged) Amount of time the buffer has been waited on for since the `FastReadbackHackWaitTimeThreshold`th wait on it by the guest

        /**
         * @brief Resets all megabuffer tracking state
         */
        void ResetMegabufferState();

      private:
        BufferDelegate *delegate;

        friend BufferView;
        friend BufferManager;

        void SetupStagedTraps();

        /**
         * @brief Enables the shadow buffer used for sequencing buffer contents independently of the GPU on the CPU
         */
        void EnableTrackedShadowDirect();

        /**
         * @return A span for the requested region that can be used to as a destination for a CPU-side buffer write
         */
        span<u8> BeginWriteCpuSequencedDirect(size_t offset, size_t size);

        /**
         * @return If GPU reads could occur using the buffer at a given moment, when true is returned the backing must not be modified by CPU writes
         */
        bool RefreshGpuReadsActiveDirect();

        /**
         * @param wait Whether to wait until GPU writes are no longer active before returning
         * @return If GPU writes of indeterminate contents could occur using the buffer at a given moment, when true is returned the backing must not be read/written on the CPU
         */
        bool RefreshGpuWritesActiveDirect(bool wait = false, const std::function<void()> &flushHostCallback = {});

        bool ValidateMegaBufferViewImplDirect(vk::DeviceSize size);

        bool ValidateMegaBufferViewImplStaged(vk::DeviceSize size);

        /**
         * @return True if megabuffering should occur for the given view, false otherwise
         */
        bool ValidateMegaBufferView(vk::DeviceSize size);

        void CopyFromImplDirect(vk::DeviceSize dstOffset,
                                Buffer *src, vk::DeviceSize srcOffset, vk::DeviceSize size,
                                UsageTracker &usageTracker, const std::function<void()> &gpuCopyCallback);

        void CopyFromImplStaged(vk::DeviceSize dstOffset,
                                Buffer *src, vk::DeviceSize srcOffset, vk::DeviceSize size,
                                UsageTracker &usageTracker, const std::function<void()> &gpuCopyCallback);

        bool WriteImplDirect(span<u8> data, vk::DeviceSize offset,
                             UsageTracker &usageTracker, const std::function<void()> &gpuCopyCallback = {});

        bool WriteImplStaged(span<u8> data, vk::DeviceSize offset, const std::function<void()> &gpuCopyCallback = {});

        void ReadImplDirect(const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize offset);

        void ReadImplStaged(bool isFirstUsage, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize offset);

        void MarkGpuDirtyImplDirect();

        void MarkGpuDirtyImplStaged();

        void MarkGpuDirtyImpl();

      public:
        void UpdateCycle(const std::shared_ptr<FenceCycle> &newCycle) {
            newCycle->ChainCycle(cycle);
            cycle = newCycle;
        }

        constexpr vk::Buffer GetBacking() {
            return backing ? backing->vkBuffer : *directBacking->vkBuffer;
        }

        /**
         * @return A span over the backing of this buffer
         * @note This operation **must** be performed only on host-only buffers since synchronization is handled internally for guest-backed buffers
         */
        span<u8> GetBackingSpan() {
            if (guest)
                throw exception("Attempted to get a span of a guest-backed buffer");
            return backing ? span<u8>(*backing) : span<u8>(*directBacking);
        }

        /**
         * @brief Creates a buffer object wrapping the guest buffer with a backing that can represent the guest buffer data
         * @note The guest mappings will not be setup until SetupGuestMappings() is called
         */
        Buffer(LinearAllocatorState<> &delegateAllocator, GPU &gpu, GuestBuffer guest, size_t id, bool direct);

        /**
         * @brief Creates a host-only Buffer which isn't backed by any guest buffer
         * @note The created buffer won't have a mirror so any operations cannot depend on a mirror existing
         */
        Buffer(LinearAllocatorState<> &delegateAllocator, GPU &gpu, vk::DeviceSize size, size_t id);

        ~Buffer();

        /**
         * @brief Acquires an exclusive lock on the buffer for the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void lock();

        /**
         * @brief Acquires an exclusive lock on the buffer for the calling thread
         * @param tag A tag to associate with the lock, future invocations with the same tag prior to the unlock will acquire the lock without waiting (A default initialised tag will disable this behaviour)
         * @return If the lock was acquired by this call as opposed to the buffer already being locked with the same tag
         * @note All locks using the same tag **must** be from the same thread as it'll only have one corresponding unlock() call
         */
        bool LockWithTag(ContextTag tag);

        /**
         * @brief Relinquishes an existing lock on the buffer by the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void unlock();

        /**
         * @brief Attempts to acquire an exclusive lock but returns immediately if it's captured by another thread
         * @note Naming is in accordance to the Lockable named requirement
         */
        bool try_lock();

        /**
         * @brief Marks the buffer as dirty on the GPU, it will be synced on the next call to SynchronizeGuest
         * @note This **must** be called after syncing the buffer to the GPU not before
         * @note The buffer **must** be locked prior to calling this
         */
        void MarkGpuDirty(UsageTracker &usageTracker);

        /**
         * @brief Prevents sequenced writes to this buffer's backing from occuring on the CPU, forcing sequencing on the GPU instead for the duration of the context. Unsequenced writes such as those from the guest can still occur however.
         * @note The buffer **must** be locked prior to calling this
         */
        void BlockSequencedCpuBackingWrites() {
            std::unique_lock lock{stateMutex, std::defer_lock};
            if (!isDirect)
                lock.lock();

            if (backingImmutability == BackingImmutability::None)
                backingImmutability = BackingImmutability::SequencedWrites;
        }

        /**
         * @brief Prevents *any* writes to this buffer's backing from occuring on the CPU, forcing sequencing on the GPU instead for the duration of the context.
         * @note The buffer **must** be locked prior to calling this
         */
        void BlockAllCpuBackingWrites() {
            std::unique_lock lock{stateMutex, std::defer_lock};
            if (!isDirect)
                lock.lock();

            backingImmutability = BackingImmutability::AllWrites;
        }

        void AllowAllBackingWrites() {
            std::unique_lock lock{stateMutex, std::defer_lock};
            if (!isDirect)
                lock.lock();

            backingImmutability = BackingImmutability::None;
        }

        /**
         * @return If sequenced writes to the backing must not occur on the CPU
         * @note The buffer **must** be locked prior to calling this
         */
        bool SequencedCpuBackingWritesBlocked() {
            std::unique_lock lock{stateMutex, std::defer_lock};
            if (!isDirect)
                lock.lock();

            return backingImmutability == BackingImmutability::SequencedWrites || backingImmutability == BackingImmutability::AllWrites;
        }

        /**
         * @return If no writes to the backing are allowed to occur on the CPU
         * @note The buffer **must** be locked prior to calling this
         */
        bool AllCpuBackingWritesBlocked() {
            std::unique_lock lock{stateMutex, std::defer_lock};
            if (!isDirect)
                lock.lock();

            return backingImmutability == BackingImmutability::AllWrites;
        }

        /**
         * @return If the cycle needs to be attached to the buffer before ending the current context
         * @note This is an alias for `SequencedCpuBackingWritesBlocked()` since this is only ever set when the backing is accessed on the GPU in some form
         * @note The buffer **must** be locked prior to calling this
         */
        bool RequiresCycleAttach() {
            return SequencedCpuBackingWritesBlocked();
        }

        /**
         * @return If the buffer is frequently locked by threads using non-ContextLocks
         */
        bool FrequentlyLocked() {
            return accumulatedCpuLockCounter >= FrequentlyLockedThreshold;
        }

        /*
         * @note The buffer **must** be locked prior to calling this
         */
        bool IsCurrentExecutionGpuDirty() {
            return currentExecutionGpuDirty;
        }

        /**
         * @brief Waits on a fence cycle if it exists till it's signalled and resets it after
         * @note The buffer **must** be locked prior to calling this
         */
        void WaitOnFence();

        /**
         * @brief Polls a fence cycle if it exists and resets it if signalled
         * @return Whether the fence cycle was signalled
         * @note The buffer **must** be locked prior to calling this
         */
        bool PollFence();

        /**
         * @brief Invalidates the Buffer on the guest and deletes the trap that backs this buffer as it is no longer necessary
         * @note This will not clear any views or delegates on the buffer, it will only remove guest mappings and delete the trap
         * @note The buffer **must** be locked prior to calling this
         */
        void Invalidate();

        /**
         * @brief Synchronizes the host buffer with the guest
         * @param skipTrap If true, setting up a CPU trap will be skipped
         * @note The buffer **must** be locked prior to calling this
         */
        void SynchronizeHost(bool skipTrap = false);

        /**
         * @brief Synchronizes the guest buffer with the host buffer
         * @param skipTrap If true, setting up a CPU trap will be skipped
         * @param nonBlocking If true, the call will return immediately if the fence is not signalled, skipping the sync
         * @return If the buffer's contents were successfully synchronized, this'll only be false on non-blocking operations or lack of a guest buffer
         * @note The buffer **must** be locked prior to calling this
         */
        bool SynchronizeGuest(bool skipTrap = false, bool nonBlocking = false);

        /**
         * @brief Synchronizes the guest buffer with the host buffer immediately, flushing GPU work if necessary
         * @param isFirstUsage If this is the first usage of this resource in the context as returned from LockWithTag(...)
         * @param flushHostCallback Callback to flush and execute all pending GPU work to allow for synchronisation of GPU dirty buffers
         * @note The buffer **must** be locked prior to calling this
         */
        void SynchronizeGuestImmediate(bool isFirstUsage, const std::function<void()> &flushHostCallback);

        /**
         * @brief Reads data at the specified offset in the buffer
         * @param isFirstUsage If this is the first usage of this resource in the context as returned from LockWithTag(...)
         * @param flushHostCallback Callback to flush and execute all pending GPU work to allow for synchronisation of GPU dirty buffers
         */
        void Read(bool isFirstUsage, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize offset);

        /**
         * @brief Writes data at the specified offset in the buffer, falling back to GPU side copies if the buffer is host immutable
         * @param gpuCopyCallback Optional callback to perform a GPU-side copy for this Write if necessary, if such a copy is needed and this is not supplied `true` will be returned to indicate that the write needs to be repeated with the callback present
         * @return Whether the write needs to be repeated with `gpuCopyCallback` provided, always false if `gpuCopyCallback` is provided
         */
        bool Write(span<u8> data, vk::DeviceSize offset, UsageTracker &usageTracker, const std::function<void()> &gpuCopyCallback = {});

        /**
         * @brief Copies a region of the src buffer into a region of this buffer
         * @note The src/dst buffers **must** be locked prior to calling this
         */
        void CopyFrom(vk::DeviceSize dstOffset,
                      Buffer *src, vk::DeviceSize srcOffset, vk::DeviceSize size,
                      UsageTracker &usageTracker, const std::function<void()> &gpuCopyCallback);

        /**
         * @return A view into this buffer with the supplied attributes
         * @note The buffer **must** be locked prior to calling this
         */
        BufferView GetView(vk::DeviceSize offset, vk::DeviceSize size);

        /**
         * @return A view into this buffer containing the given mapping, if the buffer doesn't contain the mapping an empty view will be returned
         * @note The buffer **must** be locked prior to calling this
         */
        BufferView TryGetView(span<u8> mapping);

        /*
         * @brief If megabuffering is determined to be beneficial for this buffer, allocates and copies the given view of buffer into the megabuffer (in case of cache miss), returning a binding of the allocated megabuffer region
         * @return A binding to the megabuffer allocation for the view, may be invalid if megabuffering is not beneficial
         * @note The buffer **must** be locked prior to calling this
         */
        BufferBinding TryMegaBufferView(const std::shared_ptr<FenceCycle> &pCycle, MegaBufferAllocator &allocator, ContextTag executionTag,
                                        vk::DeviceSize offset, vk::DeviceSize size);

        /**
         * @brief Increments the sequence number of the buffer, any futher calls to AcquireCurrentSequence will return this new sequence number. See the comment for `sequenceNumber`
         * @note The buffer **must** be locked prior to calling this
         * @note This **must** be called after any modifications of the backing buffer data (but not mirror)
         */
        void AdvanceSequence();

        /**
         * @param isFirstUsage If this is the first usage of this resource in the context as returned from LockWithTag(...)
         * @param flushHostCallback Callback to flush and execute all pending GPU work to allow for synchronisation of GPU dirty buffers
         * @return A span of the backing buffer contents
         * @note The returned span **must** not be written to
         * @note The buffer **must** be kept locked until the span is no longer in use
         */
        span<u8> GetReadOnlyBackingSpan(bool isFirstUsage, const std::function<void()> &flushHostCallback);

        /**
         * @brief Populates the input src and dst stage masks with appropriate read barrier parameters for the current buffer state
         */
        void PopulateReadBarrier(vk::PipelineStageFlagBits dstStage, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask);
    };

    /**
     * @brief A delegate for a strong reference to a Buffer by a BufferView which can be changed to another Buffer transparently
     */
    class BufferDelegate {
      private:
        union {
            BufferDelegate *link{};
            Buffer *buffer;
        };
        vk::DeviceSize offset{};

        bool linked{};

      public:
        BufferDelegate(Buffer *buffer);

        /**
         * @brief Follows links to get the underlying target buffer of the delegate
         */
        Buffer *GetBuffer();

        /**
         * @brief Links the delegate to target a new buffer object
         * @note Both the current target buffer object and new target buffer object **must** be locked prior to calling this
         */
        void Link(BufferDelegate *newTarget, vk::DeviceSize newOffset);

        /**
         * @return The offset of the delegate in the buffer
         * @note The target buffer **must** be locked prior to calling this
         */
        vk::DeviceSize GetOffset();
    };

    /**
     * @brief A contiguous view into a Vulkan Buffer that represents a single guest buffer (as opposed to Buffer objects which contain multiple)
     * @note The object **must** be locked prior to accessing any members as values will be mutated
     * @note This class conforms to the Lockable and BasicLockable C++ named requirements
     */
    class BufferView {
      private:
        BufferDelegate *delegate{};
        vk::DeviceSize offset{};

        /**
         * @brief Resolves the delegate's pointer chain so it directly points to the target buffer, updating offset accordingly
         * @note The view **must** be locked prior to calling this
         */
        void ResolveDelegate();

      public:
        vk::DeviceSize size{};

        BufferView();

        BufferView(BufferDelegate *delegate, vk::DeviceSize offset, vk::DeviceSize size);

        /**
         * @return A pointer to the current underlying buffer of the view
         * @note The view **must** be locked prior to calling this
         */
        Buffer *GetBuffer() const;

        /**
         * @return The offset of the view in the underlying buffer
         * @note The view **must** be locked prior to calling this
         */
        vk::DeviceSize GetOffset() const;

        /**
         * @return A binding describing the underlying buffer state at a given moment in time
         * @note The view **must** be locked prior to calling this
         * @note This is the **ONLY** function in BufferView that can be called from non-GPFIFO threads
         */
        BufferBinding GetBinding(GPU &gpu) const;

        /**
         * @note The buffer manager **MUST** be locked prior to calling this
         */
        void lock() {
            delegate->GetBuffer()->lock();
        }

        /**
         * @note The buffer manager **MUST** be locked prior to calling this
         */
        bool try_lock() {
            return delegate->GetBuffer()->try_lock();
        }

        /**
         * @note The buffer manager **MUST** be locked prior to calling this
         */
        bool LockWithTag(ContextTag tag) {
            return delegate->GetBuffer()->LockWithTag(tag);
        }

        void unlock() {
            delegate->GetBuffer()->unlock();
        }

        /**
         * @brief Reads data at the specified offset in the view
         * @note The view **must** be locked prior to calling this
         * @note See Buffer::Read
         */
        void Read(bool isFirstUsage, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize readOffset) const;

        /**
         * @brief Writes data at the specified offset in the view
         * @note The view **must** be locked prior to calling this
         * @note See Buffer::Write
         */
        bool Write(span<u8> data, vk::DeviceSize writeOffset, UsageTracker &usageTracker, const std::function<void()> &gpuCopyCallback = {}) const;

        /*
         * @brief If megabuffering is determined to be beneficial for the underlying buffer, allocates and copies this view into the megabuffer (in case of cache miss), returning a binding of the allocated megabuffer region
         * @param sizeOverride If non-zero, specifies the size of the megabuffer region to allocate and copy to, *MUST* be smaller than the size of the view
         * @note The view **must** be locked prior to calling this
         * @note See Buffer::TryMegaBufferView
         */
        BufferBinding TryMegaBuffer(const std::shared_ptr<FenceCycle> &pCycle, MegaBufferAllocator &allocator, ContextTag executionTag, size_t sizeOverride = 0) const;

        /**
         * @return A span of the backing buffer contents
         * @note The returned span **must** not be written to
         * @note The view **must** be kept locked until the span is no longer in use
         * @note See Buffer::GetReadOnlyBackingSpan
         */
        span<u8> GetReadOnlyBackingSpan(bool isFirstUsage, const std::function<void()> &flushHostCallback);

        /**
         * @brief Copies the contents of one view into this one
         * @note The src/dst views **must** be locked prior to calling this
         */
        void CopyFrom(BufferView src, UsageTracker &usageTracker, const std::function<void()> &gpuCopyCallback);

        constexpr operator bool() {
            return delegate != nullptr;
        }
    };
}
