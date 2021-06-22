// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include "memory_manager.h"

namespace skyline::gpu::memory {
    /**
     * @brief If the result isn't VK_SUCCESS then an exception is thrown
     */
    void ThrowOnFail(VkResult result, const char *function = __builtin_FUNCTION()) {
        if (result != VK_SUCCESS)
            vk::throwResultException(vk::Result(result), function);
    }

    StagingBuffer::~StagingBuffer() {
        if (vmaAllocator && vmaAllocation && vkBuffer)
            vmaDestroyBuffer(vmaAllocator, vkBuffer, vmaAllocation);
    }

    Image::~Image() {
        if (vmaAllocator && vmaAllocation && vkImage) {
            if (pointer)
                vmaUnmapMemory(vmaAllocator, vmaAllocation);
            vmaDestroyImage(vmaAllocator, vkImage, vmaAllocation);
        }
    }

    u8 *Image::data() {
        if (pointer) [[likely]]
            return pointer;
        ThrowOnFail(vmaMapMemory(vmaAllocator, vmaAllocation, reinterpret_cast<void **>(&pointer)));
        return pointer;
    }

    MemoryManager::MemoryManager(const GPU &pGpu) : gpu(pGpu) {
        auto instanceDispatcher{gpu.vkInstance.getDispatcher()};
        auto deviceDispatcher{gpu.vkDevice.getDispatcher()};
        VmaVulkanFunctions vulkanFunctions{
            .vkGetPhysicalDeviceProperties = instanceDispatcher->vkGetPhysicalDeviceProperties,
            .vkGetPhysicalDeviceMemoryProperties = instanceDispatcher->vkGetPhysicalDeviceMemoryProperties,
            .vkAllocateMemory = deviceDispatcher->vkAllocateMemory,
            .vkFreeMemory = deviceDispatcher->vkFreeMemory,
            .vkMapMemory = deviceDispatcher->vkMapMemory,
            .vkUnmapMemory = deviceDispatcher->vkUnmapMemory,
            .vkFlushMappedMemoryRanges = deviceDispatcher->vkFlushMappedMemoryRanges,
            .vkInvalidateMappedMemoryRanges = deviceDispatcher->vkInvalidateMappedMemoryRanges,
            .vkBindBufferMemory = deviceDispatcher->vkBindBufferMemory,
            .vkBindImageMemory = deviceDispatcher->vkBindImageMemory,
            .vkGetBufferMemoryRequirements = deviceDispatcher->vkGetBufferMemoryRequirements,
            .vkGetImageMemoryRequirements = deviceDispatcher->vkGetImageMemoryRequirements,
            .vkCreateBuffer = deviceDispatcher->vkCreateBuffer,
            .vkDestroyBuffer = deviceDispatcher->vkDestroyBuffer,
            .vkCreateImage = deviceDispatcher->vkCreateImage,
            .vkDestroyImage = deviceDispatcher->vkDestroyImage,
            .vkCmdCopyBuffer = deviceDispatcher->vkCmdCopyBuffer,
            .vkGetBufferMemoryRequirements2KHR = deviceDispatcher->vkGetBufferMemoryRequirements2,
            .vkGetImageMemoryRequirements2KHR = deviceDispatcher->vkGetImageMemoryRequirements2,
            .vkBindBufferMemory2KHR = deviceDispatcher->vkBindBufferMemory2,
            .vkBindImageMemory2KHR = deviceDispatcher->vkBindImageMemory2,
            .vkGetPhysicalDeviceMemoryProperties2KHR = instanceDispatcher->vkGetPhysicalDeviceMemoryProperties2,
        };
        VmaAllocatorCreateInfo allocatorCreateInfo{
            .physicalDevice = *gpu.vkPhysicalDevice,
            .device = *gpu.vkDevice,
            .instance = *gpu.vkInstance,
            .pVulkanFunctions = &vulkanFunctions,
            .vulkanApiVersion = GPU::VkApiVersion,
        };
        ThrowOnFail(vmaCreateAllocator(&allocatorCreateInfo, &vmaAllocator));
        // TODO: Use VK_KHR_dedicated_allocation when available (Should be on Adreno GPUs)
    }

    MemoryManager::~MemoryManager() {
        vmaDestroyAllocator(vmaAllocator);
    }

    std::shared_ptr<StagingBuffer> MemoryManager::AllocateStagingBuffer(vk::DeviceSize size) {
        vk::BufferCreateInfo bufferCreateInfo{
            .size = size,
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &gpu.vkQueueFamilyIndex,
        };
        VmaAllocationCreateInfo allocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_CPU_ONLY,
        };

        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        ThrowOnFail(vmaCreateBuffer(vmaAllocator, &static_cast<const VkBufferCreateInfo &>(bufferCreateInfo), &allocationCreateInfo, &buffer, &allocation, &allocationInfo));

        return std::make_shared<memory::StagingBuffer>(reinterpret_cast<u8 *>(allocationInfo.pMappedData), allocationInfo.size, vmaAllocator, buffer, allocation);
    }

    Image MemoryManager::AllocateImage(const vk::ImageCreateInfo &createInfo) {
        VmaAllocationCreateInfo allocationCreateInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        };

        VkImage image;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        ThrowOnFail(vmaCreateImage(vmaAllocator, &static_cast<const VkImageCreateInfo &>(createInfo), &allocationCreateInfo, &image, &allocation, &allocationInfo));

        return Image(vmaAllocator, image, allocation);
    }

    Image MemoryManager::AllocateMappedImage(const vk::ImageCreateInfo &createInfo) {
        VmaAllocationCreateInfo allocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_UNKNOWN,
            .memoryTypeBits = static_cast<u32>(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eDeviceLocal),
        };

        VkImage image;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        ThrowOnFail(vmaCreateImage(vmaAllocator, &static_cast<const VkImageCreateInfo &>(createInfo), &allocationCreateInfo, &image, &allocation, &allocationInfo));

        return Image(reinterpret_cast<u8 *>(allocationInfo.pMappedData), vmaAllocator, image, allocation);
    }
}
