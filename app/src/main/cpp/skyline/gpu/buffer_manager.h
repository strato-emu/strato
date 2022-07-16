// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

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
        std::vector<std::shared_ptr<Buffer>> buffers; //!< A sorted vector of all buffer mappings

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
        BufferView FindOrCreate(GuestBuffer guestMapping, ContextTag tag = {}, const std::function<void(std::shared_ptr<Buffer>, ContextLock<Buffer> &)> &attachBuffer = {});

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
