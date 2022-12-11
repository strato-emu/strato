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

    Buffer::~Buffer() {
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

    MemoryManager::MemoryManager(GPU &pGpu) : gpu{pGpu} {
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
            .vulkanApiVersion = VkApiVersion,
        };
        ThrowOnFail(vmaCreateAllocator(&allocatorCreateInfo, &vmaAllocator));
    }

    MemoryManager::~MemoryManager() {
        vmaDestroyAllocator(vmaAllocator);
    }

    std::shared_ptr<StagingBuffer> MemoryManager::AllocateStagingBuffer(vk::DeviceSize size) {
        vk::BufferCreateInfo bufferCreateInfo{
            .size = size,
            .usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
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

        return std::make_shared<memory::StagingBuffer>(reinterpret_cast<u8 *>(allocationInfo.pMappedData), size, vmaAllocator, buffer, allocation);
    }

    Buffer MemoryManager::AllocateBuffer(vk::DeviceSize size) {
        vk::BufferCreateInfo bufferCreateInfo{
            .size = size,
            .usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformTexelBuffer | vk::BufferUsageFlagBits::eStorageTexelBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransformFeedbackBufferEXT,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &gpu.vkQueueFamilyIndex,
        };
        VmaAllocationCreateInfo allocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_UNKNOWN,
            .requiredFlags = static_cast<VkMemoryPropertyFlags>(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eDeviceLocal),
        };

        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        ThrowOnFail(vmaCreateBuffer(vmaAllocator, &static_cast<const VkBufferCreateInfo &>(bufferCreateInfo), &allocationCreateInfo, &buffer, &allocation, &allocationInfo));

        return Buffer(reinterpret_cast<u8 *>(allocationInfo.pMappedData), size, vmaAllocator, buffer, allocation);
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
            .usage = VMA_MEMORY_USAGE_UNKNOWN,
            .requiredFlags = static_cast<VkMemoryPropertyFlags>(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eDeviceLocal),
        };

        VkImage image;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        ThrowOnFail(vmaCreateImage(vmaAllocator, &static_cast<const VkImageCreateInfo &>(createInfo), &allocationCreateInfo, &image, &allocation, &allocationInfo));

        return Image(vmaAllocator, image, allocation);
    }

    ImportedBuffer MemoryManager::ImportBuffer(span<u8> cpuMapping) {
        if (!gpu.traits.supportsAdrenoDirectMemoryImport)
            throw exception("Cannot import host buffers without adrenotools import support!");

        if (!adrenotools_import_user_mem(&gpu.adrenotoolsImportMapping, cpuMapping.data(), cpuMapping.size()))
            throw exception("Failed to import user memory");

        auto buffer{gpu.vkDevice.createBuffer(vk::BufferCreateInfo{
            .size = cpuMapping.size(),
            .usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformTexelBuffer | vk::BufferUsageFlagBits::eStorageTexelBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransformFeedbackBufferEXT,
            .sharingMode = vk::SharingMode::eExclusive
        })};

        auto memory{gpu.vkDevice.allocateMemory(vk::MemoryAllocateInfo{
            .allocationSize = cpuMapping.size(),
            .memoryTypeIndex = gpu.traits.hostVisibleCoherentCachedMemoryType,
        })};

        if (!adrenotools_validate_gpu_mapping(&gpu.adrenotoolsImportMapping))
            throw exception("Failed to validate GPU mapping");

        gpu.vkDevice.bindBufferMemory2({vk::BindBufferMemoryInfo{
            .buffer = *buffer,
            .memory = *memory,
            .memoryOffset = 0
        }});

        return ImportedBuffer{cpuMapping, std::move(buffer), std::move(memory)};
    }
}
