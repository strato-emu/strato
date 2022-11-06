// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "memory_manager.h"

namespace skyline::gpu {
    constexpr static vk::DeviceSize MegaBufferChunkSize{25 * 1024 * 1024}; //!< Size in bytes of a single megabuffer chunk (25MiB)

    /**
      * @brief A simple linearly allocated GPU-side buffer used to temporarily store buffer modifications allowing them to be replayed in-sequence on the GPU
      * @note This class is **not** thread-safe and any calls must be externally synchronized
      */
    class MegaBufferChunk {
      private:
        std::shared_ptr<FenceCycle> cycle; //!< Latest cycle this chunk has had allocations in
        memory::Buffer backing; //!< The GPU buffer as the backing storage for the chunk
        span<u8> freeRegion; //!< The unallocated space in the chunk

      public:
        MegaBufferChunk(GPU &gpu);

        /**
         * @brief If the chunk's cycle is is signalled, resets the free region of the megabuffer to its initial state, if it's not signalled the chunk must not be used
         * @returns True if the chunk can be reused, false otherwise
         */
        bool TryReset();

        /**
         * @brief Returns the underlying Vulkan buffer for the chunk
         */
        vk::Buffer GetBacking() const;

        std::pair<vk::DeviceSize, span<u8>> Allocate(const std::shared_ptr<FenceCycle> &newCycle, vk::DeviceSize size, bool pageAlign = false);
    };

    /**
     * @brief Allocator for megabuffer chunks that takes the usage of resources on the GPU into account
     * @note This class is not thread-safe and any calls must be externally synchronized
     */
    class MegaBufferAllocator {
      private:
        GPU &gpu;
        std::list<MegaBufferChunk> chunks; //!< A pool of all allocated megabuffer chunks, these are dynamically utilized
        decltype(chunks)::iterator activeChunk; //!< Currently active chunk of the megabuffer which is being allocated into

      public:
        /**
         * @brief A megabuffer chunk allocation
         */
        struct Allocation {
            vk::Buffer buffer; //!< The megabuffer chunk backing hat the allocation was made within
            vk::DeviceSize offset; //!< The offset of the allocation in the chunk
            span<u8> region; //!< The CPU mapped region of the allocation in the chunk

            operator bool() const {
                return offset != 0;
            }
        };

        MegaBufferAllocator(GPU &gpu);

        /**
          * @brief Allocates data in a megabuffer chunk and returns an structure describing the allocation
          * @param pageAlign Whether the pushed data should be page aligned in the megabuffer
          * @note The allocator *MUST* be locked before calling this function
          */
        Allocation Allocate(const std::shared_ptr<FenceCycle> &cycle, vk::DeviceSize size, bool pageAlign = false);

        /**
         * @brief Pushes data to a megabuffer chunk and returns an structure describing the allocation
         * @param pageAlign Whether the pushed data should be page aligned in the megabuffer
         * @note The allocator *MUST* be locked before calling this function
         */
        Allocation Push(const std::shared_ptr<FenceCycle> &cycle, span<u8> data, bool pageAlign = false);
    };
}
