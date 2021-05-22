// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vk_mem_alloc.h>
#include "fence_cycle.h"

namespace skyline::gpu::memory {
    /**
     * @brief A view into a CPU mapping of a Vulkan buffer
     * @note The mapping **should not** be used after the lifetime of the object has ended
     */
    struct StagingBuffer : public span<u8>, FenceCycleDependency {
        VmaAllocator vmaAllocator;
        VmaAllocation vmaAllocation;
        vk::Buffer vkBuffer;

        constexpr StagingBuffer(u8 *pointer, size_t size, VmaAllocator vmaAllocator, vk::Buffer vkBuffer, VmaAllocation vmaAllocation) : vmaAllocator(vmaAllocator), vkBuffer(vkBuffer), vmaAllocation(vmaAllocation), span(pointer, size) {}

        ~StagingBuffer();
    };

    /**
     * @brief An abstraction over memory operations done in Vulkan, it's used for all allocations on the host GPU
     */
    class MemoryManager {
      private:
        const GPU &gpu;
        VmaAllocator vmaAllocator{VK_NULL_HANDLE};

        /**
         * @brief If the result isn't VK_SUCCESS then an exception is thrown
         */
        static void ThrowOnFail(VkResult result, const char *function = __builtin_FUNCTION());

      public:
        MemoryManager(const GPU &gpu);

        ~MemoryManager();

        /**
         * @brief Creates a buffer which is optimized for staging (Transfer Source)
         */
        std::shared_ptr<StagingBuffer> AllocateStagingBuffer(vk::DeviceSize size);
    };
}
