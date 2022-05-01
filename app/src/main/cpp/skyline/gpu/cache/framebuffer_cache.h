// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <common.h>

namespace skyline::gpu::cache {
    using FramebufferCreateInfo = vk::StructureChain<vk::FramebufferCreateInfo, vk::FramebufferAttachmentsCreateInfo>;

    /**
     * @brief A cache for Vulkan framebuffers to avoid unnecessary recreation, optimized for both fixed image and imageless attachments
     * @note It is generally expensive to create a framebuffer on TBDRs since it involves calculating tiling memory allocations and in the case of Adreno's proprietary driver involves several kernel calls for mapping and allocating the corresponding framebuffer memory
     */
    class FramebufferCache {
      private:
        GPU &gpu;
        std::mutex mutex; //!< Synchronizes access to the cache

      private:
        /**
         * @brief An equivalent to VkFramebufferAttachmentImageInfo with more suitable semantics for storage
         * @url https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkFramebufferAttachmentImageInfo.html
         */
        struct FramebufferImagelessAttachment {
            vk::ImageCreateFlags flags;
            vk::ImageUsageFlags usage;
            u32 width;
            u32 height;
            u32 layers;
            vk::Format format;

            FramebufferImagelessAttachment(const vk::FramebufferAttachmentImageInfo &info);

            bool operator==(const FramebufferImagelessAttachment &other) const = default;
        };

        struct FramebufferCacheKey {
            vk::FramebufferCreateFlags flags;
            vk::RenderPass renderPass;
            u32 width;
            u32 height;
            u32 layers;
            std::variant<std::vector<vk::ImageView>, std::vector<FramebufferImagelessAttachment>> attachments;

            FramebufferCacheKey(const FramebufferCreateInfo &createInfo);

            bool operator==(const FramebufferCacheKey &other) const = default;
        };

        struct FramebufferHash {
            using is_transparent = std::true_type;

            size_t operator()(const FramebufferCacheKey &key) const;

            size_t operator()(const FramebufferCreateInfo &key) const;
        };

        struct FramebufferEqual {
            using is_transparent = std::true_type;

            bool operator()(const FramebufferCacheKey &lhs, const FramebufferCacheKey &rhs) const;

            bool operator()(const FramebufferCacheKey &lhs, const FramebufferCreateInfo &rhs) const;
        };

        std::unordered_map<FramebufferCacheKey, vk::raii::Framebuffer, FramebufferHash, FramebufferEqual> framebufferCache;

      public:
        FramebufferCache(GPU &gpu);

        /**
         * @note When using imageless framebuffer attachments, VkFramebufferAttachmentImageInfo **must** have a single view format
         * @note When using image framebuffer attachments, it is expected that the supplied image handle will remain stable for the cache to function
         */
        vk::Framebuffer GetFramebuffer(const FramebufferCreateInfo &createInfo);
    };
}
