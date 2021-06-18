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
    struct StagingBuffer : public span<u8>, public FenceCycleDependency {
        VmaAllocator vmaAllocator;
        VmaAllocation vmaAllocation;
        vk::Buffer vkBuffer;

        constexpr StagingBuffer(u8 *pointer, size_t size, VmaAllocator vmaAllocator, vk::Buffer vkBuffer, VmaAllocation vmaAllocation) : vmaAllocator(vmaAllocator), vkBuffer(vkBuffer), vmaAllocation(vmaAllocation), span(pointer, size) {}

        StagingBuffer(const StagingBuffer &) = delete;

        constexpr StagingBuffer(StagingBuffer &&other) : vmaAllocator(std::exchange(other.vmaAllocator, nullptr)), vmaAllocation(std::exchange(other.vmaAllocation, nullptr)), vkBuffer(std::exchange(other.vkBuffer, {})) {}

        StagingBuffer &operator=(const StagingBuffer &) = delete;

        StagingBuffer &operator=(StagingBuffer &&) = default;

        ~StagingBuffer();
    };

    /**
     * @brief A Vulkan image which VMA allocates and manages the backing memory for
     */
    struct Image {
      private:
        u8 *pointer{};

      public:
        VmaAllocator vmaAllocator;
        VmaAllocation vmaAllocation;
        vk::Image vkImage;

        constexpr Image(VmaAllocator vmaAllocator, vk::Image vkImage, VmaAllocation vmaAllocation) : vmaAllocator(vmaAllocator), vkImage(vkImage), vmaAllocation(vmaAllocation) {}

        constexpr Image(u8 *pointer, VmaAllocator vmaAllocator, vk::Image vkImage, VmaAllocation vmaAllocation) : pointer(pointer), vmaAllocator(vmaAllocator), vkImage(vkImage), vmaAllocation(vmaAllocation) {}

        Image(const Image &) = delete;

        constexpr Image(Image &&other) : pointer(std::exchange(other.pointer, nullptr)), vmaAllocator(std::exchange(other.vmaAllocator, nullptr)), vmaAllocation(std::exchange(other.vmaAllocation, nullptr)), vkImage(std::exchange(other.vkImage, {})) {}

        Image &operator=(const Image &) = delete;

        Image &operator=(Image &&) = default;

        ~Image();

        /**
         * @return A pointer to a mapping of the image on the CPU
         * @note If the image isn't already mapped on the CPU, this creates a mapping for it
         */
        u8 *data();
    };

    /**
     * @brief An abstraction over memory operations done in Vulkan, it's used for all allocations on the host GPU
     */
    class MemoryManager {
      private:
        const GPU &gpu;
        VmaAllocator vmaAllocator{VK_NULL_HANDLE};

      public:
        MemoryManager(const GPU &gpu);

        ~MemoryManager();

        /**
         * @brief Creates a buffer which is optimized for staging (Transfer Source)
         */
        std::shared_ptr<StagingBuffer> AllocateStagingBuffer(vk::DeviceSize size);

        /**
         * @brief Creates an image which is allocated and deallocated using RAII
         */
        Image AllocateImage(const vk::ImageCreateInfo &createInfo);

        /**
         * @brief Creates an image which is allocated and deallocated using RAII and is optimal for being mapped on the CPU
         */
        Image AllocateMappedImage(const vk::ImageCreateInfo &createInfo);
    };
}
