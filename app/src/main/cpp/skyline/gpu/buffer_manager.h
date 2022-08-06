// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/segment_table.h>
#include "buffer.h"

namespace skyline::gpu {
    class MegaBuffer;

    /**
     * @brief The Buffer Manager is responsible for maintaining a global view of buffers being mapped from the guest to the host, any lookups and creation of host buffer from equivalent guest buffer alongside reconciliation of any overlaps with existing textures
     */
    class BufferManager {
      private:
        GPU &gpu;
        std::mutex mutex; //!< Synchronizes access to the buffer mappings
        std::vector<std::shared_ptr<Buffer>> bufferMappings; //!< A sorted vector of all buffer mappings

        static constexpr size_t AddressSpaceSize{1ULL << 39}; //!< The size of the guest CPU AS in bytes
        static constexpr size_t PageSizeBits{12}; //!< The size of a single page of the guest CPU AS as a power of two (4 KiB == 1 << 12)
        static constexpr size_t L2EntryGranularity{19}; //!< The amount of AS (in bytes) a single L2 PTE covers (512 KiB == 1 << 19)
        SegmentTable<Buffer*, AddressSpaceSize, PageSizeBits, L2EntryGranularity> bufferTable; //!< A page table of all buffer mappings for O(1) lookups on full matches

        std::mutex megaBufferMutex; //!< Synchronizes access to the allocated megabuffers

        friend class MegaBuffer;

        /**
         * @brief A wrapper around a buffer which can be utilized as backing storage for a megabuffer and can track its state to avoid concurrent usage
         */
        struct MegaBufferSlot {
            std::atomic_flag active{true}; //!< If the megabuffer is currently being utilized, we want to construct a buffer as active
            std::shared_ptr<FenceCycle> cycle; //!< The latest cycle on the fence, all waits must be performed through this

            memory::Buffer backing; //!< The GPU buffer as the backing storage for the megabuffer

            MegaBufferSlot(GPU &gpu);
        };

        /**
         * @brief A wrapper around a Buffer which locks it with the specified ContextTag
         */
        struct LockedBuffer {
            std::shared_ptr<Buffer> buffer;
            ContextLock<Buffer> lock;
            std::unique_lock<std::recursive_mutex> stateLock;

            LockedBuffer(std::shared_ptr<Buffer> pBuffer, ContextTag tag);

            Buffer *operator->() const;

            std::shared_ptr<Buffer> &operator*();
        };

        using LockedBuffers = boost::container::small_vector<LockedBuffer, 4>;

        /**
         * @return A vector of buffers locked with the supplied tag which are contained within the supplied range
         */
        LockedBuffers Lookup(span<u8> range, ContextTag tag);

        /**
         * @brief Inserts the supplied buffer into the map based on its guest address
         * @note The supplied buffer **must** have a valid guest mapping
         */
        void InsertBuffer(std::shared_ptr<Buffer> buffer);

        /**
         * @brief Deletes the supplied buffer from the map, the lifetime of the buffer will no longer be extended by the map
         * @note The supplied buffer **must** have a valid guest mapping
         */
        void DeleteBuffer(const std::shared_ptr<Buffer> &buffer);

        /**
         * @brief Coalesce the supplied buffers into a single buffer encompassing the specified range and locks it with the supplied tag
         * @param range The range of memory that the newly created buffer will cover, this will be extended to cover the entirety of the supplied buffers automatically and can be null
         * @note The supplied buffers **must** be in the map and locked with the supplied tag
         */
        LockedBuffer CoalesceBuffers(span<u8> range, const LockedBuffers &srcBuffers, ContextTag tag);

        /**
         * @return If the end of the supplied buffer is less than the supplied pointer
         */
        static bool BufferLessThan(const std::shared_ptr<Buffer> &it, u8 *pointer);

      public:
        std::list<MegaBufferSlot> megaBuffers; //!< A pool of all allocated megabuffers, these are dynamically utilized

        BufferManager(GPU &gpu);

        /**
         * @brief Acquires an exclusive lock on the texture for the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void lock();

        /**
         * @brief Relinquishes an existing lock on the texture by the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void unlock();

        /**
         * @brief Attempts to acquire an exclusive lock but returns immediately if it's captured by another thread
         * @note Naming is in accordance to the Lockable named requirement
         */
        bool try_lock();

        /**
         * @param attachBuffer A function that attaches the buffer to the current context, this'll be called when coalesced buffers are merged into the current buffer
         * @return A pre-existing or newly created Buffer object which covers the supplied mappings
         * @note The buffer manager **must** be locked prior to calling this
         */
        BufferView FindOrCreate(GuestBuffer guestMapping, ContextTag tag = {}, const std::function<void(std::shared_ptr<Buffer>, ContextLock<Buffer> &&)> &attachBuffer = {});

        /**
         * @return A dynamically allocated megabuffer which can be used to store buffer modifications allowing them to be replayed in-sequence on the GPU
         * @note This object **must** be destroyed to be reclaimed by the manager and prevent a memory leak
         * @note The buffer manager **doesn't** need to be locked prior to calling this
         */
        MegaBuffer AcquireMegaBuffer(const std::shared_ptr<FenceCycle> &cycle);
    };

    /**
     * @brief A simple linearly allocated GPU-side buffer used to temporarily store buffer modifications allowing them to be replayed in-sequence on the GPU
     * @note This class is **not** thread-safe and any calls must be externally synchronized
     */
    class MegaBuffer {
      private:
        BufferManager::MegaBufferSlot *slot;
        span<u8> freeRegion; //!< The unallocated space in the megabuffer

      public:
        MegaBuffer(BufferManager::MegaBufferSlot &slot);

        ~MegaBuffer();

        MegaBuffer &operator=(MegaBuffer &&other);

        /**
         * @return If any allocations into the megabuffer were done at the time of the call
         */
        bool WasUsed();

        /**
         * @brief Replaces the cycle associated with the underlying megabuffer with the supplied cycle
         * @note The megabuffer must **NOT** have any dependencies that aren't conveyed by the supplied cycle
         */
        void ReplaceCycle(const std::shared_ptr<FenceCycle> &cycle);

        /**
         * @brief Resets the free region of the megabuffer to its initial state, data is left intact but may be overwritten
         */
        void Reset();

        /**
         * @brief Returns the underlying Vulkan buffer for the megabuffer
         */
        vk::Buffer GetBacking() const;

        /**
         * @brief Pushes data to the megabuffer and returns the offset at which it was written
         * @param pageAlign Whether the pushed data should be page aligned in the megabuffer
         */
        vk::DeviceSize Push(span<u8> data, bool pageAlign = false);
    };
}
