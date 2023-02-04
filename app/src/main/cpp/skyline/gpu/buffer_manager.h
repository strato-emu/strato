// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/trace.h>
#include <common/linear_allocator.h>
#include <common/segment_table.h>
#include <common/spin_lock.h>
#include "buffer.h"

namespace skyline::gpu {
    /**
     * @brief The Buffer Manager is responsible for maintaining a global view of buffers being mapped from the guest to the host, any lookups and creation of host buffer from equivalent guest buffer alongside reconciliation of any overlaps with existing textures
     */
    class BufferManager {
      private:
        GPU &gpu;
        std::vector<std::shared_ptr<Buffer>> bufferMappings; //!< A sorted vector of all buffer mappings
        LinearAllocatorState<> delegateAllocatorState; //!< Linear allocator used to allocate buffer delegates
        size_t nextBufferId{}; //!< The next unique buffer id to be assigned

        static constexpr size_t L2EntryGranularity{19}; //!< The amount of AS (in bytes) a single L2 PTE covers (512 KiB == 1 << 19)
        SegmentTable<Buffer *, constant::AddressSpaceSize, constant::PageSizeBits, L2EntryGranularity> bufferTable; //!< A page table of all buffer mappings for O(1) lookups on full matches

        /**
         * @brief A wrapper around a Buffer which locks it with the specified ContextTag
         */
        struct LockedBuffer {
            std::shared_ptr<Buffer> buffer;
            ContextLock<Buffer> lock;
            std::unique_lock<RecursiveSpinLock> stateLock;

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
        SpinLock recreationMutex;

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
        BufferView FindOrCreateImpl(GuestBuffer guestMapping, ContextTag tag, const std::function<void(std::shared_ptr<Buffer>, ContextLock<Buffer> &&)> &attachBuffer);

        BufferView FindOrCreate(GuestBuffer guestMapping, ContextTag tag = {}, const std::function<void(std::shared_ptr<Buffer>, ContextLock<Buffer> &&)> &attachBuffer = {}) {
            TRACE_EVENT("gpu", "BufferManager::FindOrCreate");
            auto lookupBuffer{bufferTable[guestMapping.begin().base()]};
            if (lookupBuffer != nullptr)
                if (auto view{lookupBuffer->TryGetView(guestMapping)}; view)
                    return view;

            return FindOrCreateImpl(guestMapping, tag, attachBuffer);
        }
    };
}
