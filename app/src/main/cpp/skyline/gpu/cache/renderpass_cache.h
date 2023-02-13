// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <common.h>

namespace skyline::gpu::cache {
    /**
     * @brief A cache for Vulkan render passes to avoid unnecessary recreation and attain stability in handles for subsequent caches
     */
    class RenderPassCache {
      private:
        GPU &gpu;
        std::mutex mutex; //!< Synchronizes access to the cache

        /**
         * @url https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkSubpassDescription.html
         */
        struct SubpassDescription {
            vk::SubpassDescriptionFlags flags;
            vk::PipelineBindPoint pipelineBindPoint;
            std::vector<vk::AttachmentReference> inputAttachments;
            std::vector<vk::AttachmentReference> colorAttachments;
            std::vector<vk::AttachmentReference> resolveAttachments;
            std::optional<vk::AttachmentReference> depthStencilAttachment;
            std::vector<u32> preserveAttachments;

            SubpassDescription(const vk::SubpassDescription &description);

            bool operator==(const SubpassDescription &rhs) const = default;
        };

        /**
         * @url https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkRenderPassCreateInfo.html
         */
        struct RenderPassMetadata {
            std::vector<vk::AttachmentDescription> attachments;
            std::vector<SubpassDescription> subpasses;
            std::vector<vk::SubpassDependency> dependencies;

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
