// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <unordered_set>
#include <boost/functional/hash.hpp>
#include <common/lockable_shared_ptr.h>
#include <common/linear_allocator.h>
#include <nce.h>
#include <gpu/tag_allocator.h>
#include "megabuffer.h"
#include "memory_manager.h"

namespace skyline::gpu {
    using GuestBuffer = span<u8>; //!< The CPU mapping for the guest buffer, multiple mappings for buffers aren't supported since overlaps cannot be reconciled

    struct BufferView;
    class BufferManager;
    class MegaBuffer;

    /**
     * @brief A buffer which is backed by host constructs while being synchronized with the underlying guest buffer
     * @note This class conforms to the Lockable and BasicLockable C++ named requirements
     */
    class Buffer : public std::enable_shared_from_this<Buffer> {
      private:
        GPU &gpu;
        std::mutex mutex; //!< Synchronizes any mutations to the buffer or its backing
        std::atomic<ContextTag> tag{}; //!< The tag associated with the last lock call
        memory::Buffer backing;
        std::optional<GuestBuffer> guest;
        std::shared_ptr<FenceCycle> cycle{}; //!< A fence cycle for when any host operation mutating the buffer has completed, it must be waited on prior to any mutations to the backing

        span<u8> mirror{}; //!< A contiguous mirror of all the guest mappings to allow linear access on the CPU
        span<u8> alignedMirror{}; //!< The mirror mapping aligned to page size to reflect the full mapping
        std::optional<nce::NCE::TrapHandle> trapHandle{}; //!< The handle of the traps for the guest mappings

        enum class DirtyState {
            Clean, //!< The CPU mappings are in sync with the GPU buffer
            CpuDirty, //!< The CPU mappings have been modified but the GPU buffer is not up to date
            GpuDirty, //!< The GPU buffer has been modified but the CPU mappings have not been updated
        } dirtyState{DirtyState::CpuDirty}; //!< The state of the CPU mappings with respect to the GPU buffer

        enum class BackingImmutability {
            None, //!< Backing can be freely written to and read from
            SequencedWrites, //!< Sequenced writes must not modify the backing on the CPU due to it being read directly on the GPU, but non-sequenced writes can freely occur (SynchroniseHost etc)
            AllWrites //!< No CPU writes to the backing can be performed, all must be sequenced on the GPU or delayed till this is no longer the case
        } backingImmutability{}; //!< Describes how the buffer backing should be accessed by the current context
        std::recursive_mutex stateMutex; //!< Synchronizes access to the dirty state and backing immutability

        bool everHadInlineUpdate{}; //!< Whether the buffer has ever had an inline update since it was created, if this is set then megabuffering will be attempted by views to avoid the cost of inline GPU updates

      public:
        /**
         * @brief Storage for all metadata about a specific view into the buffer, used to prevent redundant view creation and duplication of VkBufferView(s)
         */
        struct BufferViewStorage {
            vk::DeviceSize offset;
            vk::DeviceSize size;
            vk::Format format;

            // These are not accounted for in hash nor operator== since they are not an inherent property of the view, but they are required nonetheless for megabuffering on a per-view basis
            mutable u64 lastAcquiredSequence{}; //!< The last sequence number for the attached buffer that the megabuffer copy of this view was acquired from, if this is equal to the current sequence of the attached buffer then the copy at `megabufferOffset` is still valid
            mutable MegaBufferAllocator::Allocation megaBufferAllocation; //!< Allocation for the current copy of the view in the megabuffer (if any), 0 if no copy exists and this is only valid if `lastAcquiredSequence` is equal to the current sequence of the attached buffer

            BufferViewStorage(vk::DeviceSize offset, vk::DeviceSize size, vk::Format format);

            bool operator==(const BufferViewStorage &other) const {
                return other.offset == offset && other.size == size && other.format == format;
            }
        };

        static constexpr u64 InitialSequenceNumber{1}; //!< Sequence number that all buffers start off with

      private:
        /**
         * @brief Hash function for BufferViewStorage to be used in the views set
         */
        struct BufferViewStorageHash {
            size_t operator()(const BufferViewStorage &entry) const noexcept {
                size_t seed{};
                boost::hash_combine(seed, entry.offset);
                boost::hash_combine(seed, entry.size);
                boost::hash_combine(seed, entry.format);

                // The mutable fields {lastAcquiredSequence, megabufferOffset} are deliberately ignored
                return seed;
            }
        };

        std::unordered_set<BufferViewStorage, BufferViewStorageHash> views; //!< BufferViewStorage(s) that are backed by this Buffer, used for storage and repointing to a new Buffer on deletion

        u64 sequenceNumber{InitialSequenceNumber}; //!< Sequence number that is incremented after all modifications to the host side `backing` buffer, used to prevent redundant copies of the buffer being stored in the megabuffer by views

      public:
        /**
         * @brief A delegate for a strong reference to a Buffer by a BufferView which can be changed to another Buffer transparently
         * @note This class conforms to the Lockable and BasicLockable C++ named requirements
         */
        struct BufferDelegate {
            LockableSharedPtr<Buffer> buffer;
            const Buffer::BufferViewStorage *view;
            bool attached{};
            using UsageCallback = std::function<void(const BufferViewStorage &, const std::shared_ptr<Buffer> &)>;
            std::optional<std::vector<UsageCallback, LinearAllocator<UsageCallback>>> usageCallbacks;
            std::list<BufferDelegate *>::iterator iterator;

            BufferDelegate(std::shared_ptr<Buffer> buffer, const Buffer::BufferViewStorage *view);

            ~BufferDelegate();

            void lock();

            bool LockWithTag(ContextTag tag);

            void unlock();

            bool try_lock();
        };

      private:
        std::list<BufferDelegate *> delegates; //!< The reference delegates for this buffer, used to prevent the buffer from being deleted while it is still in use

        friend BufferView;
        friend BufferManager;

        /**
         * @brief Sets up mirror mappings for the guest mappings, this must be called after construction for the mirror to be valid
         */
        void SetupGuestMappings();

      public:
        void UpdateCycle(const std::shared_ptr<FenceCycle> &newCycle) {
            std::scoped_lock lock{stateMutex};
            newCycle->ChainCycle(cycle);
            cycle = newCycle;
        }

        constexpr vk::Buffer GetBacking() {
            return backing.vkBuffer;
        }

        /**
         * @return A span over the backing of this buffer
         * @note This operation **must** be performed only on host-only buffers since synchronization is handled internally for guest-backed buffers
         */
        span<u8> GetBackingSpan() {
            if (guest)
                throw exception("Attempted to get a span of a guest-backed buffer");
            return span<u8>(backing);
        }

        /**
         * @brief Creates a buffer object wrapping the guest buffer with a backing that can represent the guest buffer data
         * @note The guest mappings will not be setup until SetupGuestMappings() is called
         */
        Buffer(GPU &gpu, GuestBuffer guest);

        /**
         * @brief Creates a host-only Buffer which isn't backed by any guest buffer
         * @note The created buffer won't have a mirror so any operations cannot depend on a mirror existing
         */
        Buffer(GPU &gpu, vk::DeviceSize size);

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
        void MarkGpuDirty();

        /**
         * @brief Prevents sequenced writes to this buffer's backing from occuring on the CPU, forcing sequencing on the GPU instead for the duration of the context. Unsequenced writes such as those from the guest can still occur however.
         * @note The buffer **must** be locked prior to calling this
         */
        void BlockSequencedCpuBackingWrites() {
            std::scoped_lock lock{stateMutex};
            if (backingImmutability == BackingImmutability::None)
                backingImmutability = BackingImmutability::SequencedWrites;
        }

        /**
         * @brief Prevents *any* writes to this buffer's backing from occuring on the CPU, forcing sequencing on the GPU instead for the duration of the context.
         * @note The buffer **must** be locked prior to calling this
         */
        void BlockAllCpuBackingWrites() {
            std::scoped_lock lock{stateMutex};
            backingImmutability = BackingImmutability::AllWrites;
        }

        /**
         * @return If sequenced writes to the backing must not occur on the CPU
         * @note The buffer **must** be locked prior to calling this
         */
        bool SequencedCpuBackingWritesBlocked() {
            std::scoped_lock lock{stateMutex};
            return backingImmutability == BackingImmutability::SequencedWrites || backingImmutability == BackingImmutability::AllWrites;
        }

        /**
         * @return If no writes to the backing are allowed to occur on the CPU
         * @note The buffer **must** be locked prior to calling this
         */
        bool AllCpuBackingWritesBlocked() {
            std::scoped_lock lock{stateMutex};
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
         * @note The buffer **must** be locked prior to calling this
         */
        bool EverHadInlineUpdate() const {
            return everHadInlineUpdate;
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
         * @param isFirstUsage If this is the first usage of this resource in the context as returned from LockWithTag(...)
         * @param flushHostCallback Callback to flush and execute all pending GPU work to allow for synchronisation of GPU dirty buffers
         * @param gpuCopyCallback Callback to perform a GPU-side copy for this Write
         */
        void Write(bool isFirstUsage, const std::function<void()> &flushHostCallback, const std::function<void()> &gpuCopyCallback, span<u8> data, vk::DeviceSize offset);

        /**
         * @return A cached or newly created view into this buffer with the supplied attributes
         * @note The buffer **must** be locked prior to calling this
         */
        BufferView GetView(vk::DeviceSize offset, vk::DeviceSize size, vk::Format format = {});

        /**
         * @brief Attempts to return the current sequence number and prepare the buffer for read accesses from the returned span
         * @return The current sequence number and a span of the buffers guest mirror given that the buffer is not GPU dirty, if it is then a zero sequence number is returned
         * @note The contents of the returned span can be cached safely given the sequence number is unchanged
         * @note The buffer **must** be locked prior to calling this
         * @note An implicit CPU -> GPU sync will be performed when calling this, an immediate GPU -> CPU sync will also be attempted if the buffer is GPU dirty
         */
        std::pair<u64, span<u8>> AcquireCurrentSequence();

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
    };

    /**
     * @brief A contiguous view into a Vulkan Buffer that represents a single guest buffer (as opposed to Buffer objects which contain multiple)
     * @note The object **must** be locked prior to accessing any members as values will be mutated
     * @note This class conforms to the Lockable and BasicLockable C++ named requirements
     */
    struct BufferView {
        constexpr static vk::DeviceSize MegaBufferingDisableThreshold{1024 * 128}; //!< The threshold at which the view is considered to be too large to be megabuffered (128KiB)

        std::shared_ptr<Buffer::BufferDelegate> bufferDelegate;

        BufferView(std::shared_ptr<Buffer> buffer, const Buffer::BufferViewStorage *view);

        constexpr BufferView(nullptr_t = nullptr) : bufferDelegate(nullptr) {}

        /**
         * @brief Acquires an exclusive lock on the buffer for the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void lock() const {
            bufferDelegate->lock();
        }

        /**
         * @brief Acquires an exclusive lock on the buffer for the calling thread
         * @param tag A tag to associate with the lock, future invocations with the same tag prior to the unlock will acquire the lock without waiting (A default initialised tag will disable this behaviour)
         * @return If the lock was acquired by this call as opposed to the buffer already being locked with the same tag
         * @note All locks using the same tag **must** be from the same thread as it'll only have one corresponding unlock() call
         */
        bool LockWithTag(ContextTag tag) const {
            return bufferDelegate->LockWithTag(tag);
        }

        /**
         * @brief Relinquishes an existing lock on the buffer by the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void unlock() const {
            bufferDelegate->unlock();
        }

        /**
         * @brief Attempts to acquire an exclusive lock but returns immediately if it's captured by another thread
         * @note Naming is in accordance to the Lockable named requirement
         */
        bool try_lock() const {
            return bufferDelegate->try_lock();
        }

        constexpr operator bool() const {
            return bufferDelegate != nullptr;
        }

        /**
         * @note The buffer **must** be locked prior to calling this
         */
        Buffer::BufferDelegate *operator->() const {
            return bufferDelegate.get();
        }

        /**
         * @brief Registers a callback for a usage of this view, it may be called multiple times due to the view being recreated with different backings
         * @note This will force the buffer to be host immutable for the current cycle, preventing megabuffering and requiring slower GPU inline writes instead
         * @note The callback will be automatically called the first time after registration
         * @note The view **must** be locked prior to calling this
         */
        void RegisterUsage(LinearAllocatorState<> &allocator, const std::shared_ptr<FenceCycle> &cycle, Buffer::BufferDelegate::UsageCallback usageCallback);

        /**
         * @brief Reads data at the specified offset in the view
         * @note The view **must** be locked prior to calling this
         * @note See Buffer::Read
         */
        void Read(bool isFirstUsage, const std::function<void()> &flushHostCallback, span<u8> data, vk::DeviceSize offset) const;

        /**
         * @brief Writes data at the specified offset in the view
         * @note The view **must** be locked prior to calling this
         * @note See Buffer::Write
         */
        void Write(bool isFirstUsage, const std::shared_ptr<FenceCycle> &cycle, const std::function<void()> &flushHostCallback, const std::function<void()> &gpuCopyCallback, span<u8> data, vk::DeviceSize offset) const;

        /**
         * @brief If megabuffering is beneficial for the current buffer, pushes its contents into the megabuffer and returns the offset of the pushed data
         * @return The megabuffer allocation for the buffer, may be invalid if megabuffering is not beneficial
         * @note The view **must** be locked prior to calling this
         */
        MegaBufferAllocator::Allocation AcquireMegaBuffer(const std::shared_ptr<FenceCycle> &pCycle, MegaBufferAllocator &allocator) const;

        /**
         * @return A span of the backing buffer contents
         * @note The returned span **must** not be written to
         * @note The view **must** be kept locked until the span is no longer in use
         * @note See Buffer::GetReadOnlyBackingSpan
         */
        span<u8> GetReadOnlyBackingSpan(bool isFirstUsage, const std::function<void()> &flushHostCallback);
    };
}
