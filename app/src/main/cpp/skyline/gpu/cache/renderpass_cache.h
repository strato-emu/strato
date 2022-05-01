// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::gpu::cache {
    /**
     * @brief A cache for Vulkan render passes to avoid unnecessary recreation and attain stability in handles for subsequent caches
     */
    class RenderPassCache {
      private:
        GPU &gpu;
        std::mutex mutex; //!< Synchronizes access to the cache

        using AttachmentReference = u32;

        /**
         * @brief All unique metadata in a single subpass for a compatible render pass according to Render Pass Compatibility clause in the Vulkan specification
         * @url https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#renderpass-compatibility
         * @url https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkSubpassDescription.html
         */
        struct SubpassMetadata {
            std::vector<AttachmentReference> inputAttachments;
            std::vector<AttachmentReference> colorAttachments;
            std::vector<AttachmentReference> resolveAttachments;
            std::optional<AttachmentReference> depthStencilAttachment;
            std::vector<AttachmentReference> preserveAttachments;

            bool operator==(const SubpassMetadata &rhs) const = default;
        };

        /**
         * @brief All unique metadata in a render pass for a corresponding compatible render pass according to Render Pass Compatibility clause in the Vulkan specification
         * @url https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#renderpass-compatibility
         * @url https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkRenderPassCreateInfo.html
         */
        struct RenderPassMetadata {
            std::vector<AttachmentMetadata> attachments;
            std::vector<SubpassMetadata> subpasses;

            RenderPassMetadata(const vk::RenderPassCreateInfo &createInfo);

            bool operator==(const RenderPassMetadata &other) const = default;
        };

        struct RenderPassHash {
            using is_transparent = std::true_type;

            size_t operator()(const RenderPassMetadata &key) const;

            size_t operator()(const vk::RenderPassCreateInfo &key) const;
        };

        struct RenderPassEqual {
            using is_transparent = std::true_type;

            bool operator()(const RenderPassMetadata &lhs, const RenderPassMetadata &rhs) const;

            bool operator()(const RenderPassMetadata &lhs, const vk::RenderPassCreateInfo &rhs) const;
        };

        std::unordered_map<RenderPassMetadata, vk::raii::RenderPass, RenderPassHash, RenderPassEqual> renderPassCache;

      public:
        RenderPassCache(GPU &gpu);

        vk::RenderPass GetRenderPass(const vk::RenderPassCreateInfo &createInfo);
    };
}
