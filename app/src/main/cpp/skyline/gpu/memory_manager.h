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
    struct Buffer : public span<u8> {
        VmaAllocator vmaAllocator;
        VmaAllocation vmaAllocation;
        vk::Buffer vkBuffer;

        constexpr Buffer(u8 *pointer, size_t size, VmaAllocator vmaAllocator, vk::Buffer vkBuffer, VmaAllocation vmaAllocation)
            : vmaAllocator(vmaAllocator),
              vkBuffer(vkBuffer),
              vmaAllocation(vmaAllocation),
              span(pointer, size) {}

        Buffer(const Buffer &) = delete;

        constexpr Buffer(Buffer &&other)
            : vmaAllocator(std::exchange(other.vmaAllocator, nullptr)),
              vmaAllocation(std::exchange(other.vmaAllocation, nullptr)),
              vkBuffer(std::exchange(other.vkBuffer, {})),
              span(other) {}

        Buffer &operator=(const Buffer &) = delete;

        Buffer &operator=(Buffer &&) = default;

        ~Buffer();
    };

    /**
     * @brief A Buffer that can be independently attached to a fence cycle
     */
    class StagingBuffer : public Buffer {
        using Buffer::Buffer;
    };

    /**
     * @brief A buffer that directly owns it's own memory
     */
    struct ImportedBuffer : public span<u8> {
        vk::raii::Buffer vkBuffer;
        vk::raii::DeviceMemory vkMemory;

        ImportedBuffer(span<u8> data, vk::raii::Buffer vkBuffer, vk::raii::DeviceMemory vkMemory)
            : vkBuffer{std::move(vkBuffer)},
              vkMemory{std::move(vkMemory)},
              span{data} {}

        ImportedBuffer(const ImportedBuffer &) = delete;

        ImportedBuffer(ImportedBuffer &&other)
            : vkBuffer{std::move(other.vkBuffer)},
              vkMemory{std::move(other.vkMemory)},
              span{other} {}

        ImportedBuffer &operator=(const ImportedBuffer &) = delete;

        ImportedBuffer &operator=(ImportedBuffer &&) = default;
    };

    /**
     * @brief A Vulkan image which VMA allocates and manages the backing memory for
     * @note Any images created with VMA_ALLOCATION_CREATE_MAPPED_BIT must not be utilized with this since it'll unconditionally unmap when a pointer is present which is illegal when an image was created with that flag as unmapping will be automatically performed on image deletion
     */
    struct Image {
      private:
        u8 *pointer{};

      public:
        VmaAllocator vmaAllocator;
        VmaAllocation vmaAllocation;
        vk::Image vkImage;

        constexpr Image(VmaAllocator vmaAllocator, vk::Image vkImage, VmaAllocation vmaAllocation)
            : vmaAllocator(vmaAllocator),
              vkImage(vkImage),
              vmaAllocation(vmaAllocation) {}

        constexpr Image(u8 *pointer, VmaAllocator vmaAllocator, vk::Image vkImage, VmaAllocation vmaAllocation)
            : pointer(pointer),
              vmaAllocator(vmaAllocator),
              vkImage(vkImage),
              vmaAllocation(vmaAllocation) {}

        Image(const Image &) = delete;

        constexpr Image(Image &&other)
            : pointer(std::exchange(other.pointer, nullptr)),
              vmaAllocator(std::exchange(other.vmaAllocator, nullptr)),
              vmaAllocation(std::exchange(other.vmaAllocation, nullptr)),
              vkImage(std::exchange(other.vkImage, {})) {}

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
        GPU &gpu;
        VmaAllocator vmaAllocator{VK_NULL_HANDLE};

      public:
        MemoryManager(GPU &gpu);

        ~MemoryManager();

        /**
         * @brief Creates a buffer which is optimized for staging (Transfer Source)
         */
        std::shared_ptr<StagingBuffer> AllocateStagingBuffer(vk::DeviceSize size);

        /**
         * @brief Creates a buffer with a CPU mapping and all usage flags
         */
        Buffer AllocateBuffer(vk::DeviceSize size);

        /**
         * @brief Creates an image which is allocated and deallocated using RAII
         */
        Image AllocateImage(const vk::ImageCreateInfo &createInfo);

        /**
         * @brief Creates an image which is allocated and deallocated using RAII and is optimal for being mapped on the CPU
         */
        Image AllocateMappedImage(const vk::ImageCreateInfo &createInfo);

        /**
         * @brief Maps the input CPU mapped region into a new buffer
         */
        ImportedBuffer ImportBuffer(span<u8> cpuMapping);
    };
}
