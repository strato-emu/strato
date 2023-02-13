// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <boost/functional/hash.hpp>
#include <gpu.h>
#include "renderpass_cache.h"

namespace skyline::gpu::cache {
    RenderPassCache::RenderPassCache(gpu::GPU &gpu) : gpu{gpu} {}

    #define VEC_CPY(pointer, size) description.pointer, description.pointer + description.size

    RenderPassCache::SubpassDescription::SubpassDescription(const vk::SubpassDescription &description)
        : flags{description.flags},
          pipelineBindPoint{description.pipelineBindPoint},
          inputAttachments{VEC_CPY(pInputAttachments, inputAttachmentCount)},
          colorAttachments{VEC_CPY(pColorAttachments, colorAttachmentCount)},
          resolveAttachments{description.pResolveAttachments, description.pResolveAttachments + (description.pResolveAttachments ? description.colorAttachmentCount : 0)},
          depthStencilAttachment{description.pDepthStencilAttachment ? *description.pDepthStencilAttachment : std::optional<vk::AttachmentReference>{}},
          preserveAttachments{VEC_CPY(pPreserveAttachments, preserveAttachmentCount)} {}

    #undef VEC_CPY

    RenderPassCache::RenderPassMetadata::RenderPassMetadata(const vk::RenderPassCreateInfo &createInfo)
        : attachments{createInfo.pAttachments, createInfo.pAttachments + createInfo.attachmentCount},
          subpasses{createInfo.pSubpasses, createInfo.pSubpasses + createInfo.subpassCount},
          dependencies{createInfo.pDependencies, createInfo.pDependencies + createInfo.dependencyCount} {}

    #define HASH(x) boost::hash_combine(hash, x)

    size_t RenderPassCache::RenderPassHash::operator()(const RenderPassMetadata &key) const {
        size_t hash{};

        HASH(key.attachments.size());
        for (const auto &attachment : key.attachments) {
            HASH(static_cast<VkAttachmentDescriptionFlags>(attachment.flags));
            HASH(attachment.format);
            HASH(attachment.samples);
            HASH(attachment.loadOp);
            HASH(attachment.storeOp);
            HASH(attachment.stencilLoadOp);
            HASH(attachment.stencilStoreOp);
            HASH(attachment.initialLayout);
            HASH(attachment.finalLayout);
        }

        HASH(key.subpasses.size());
        for (const auto &subpass : key.subpasses) {
            HASH(static_cast<VkSubpassDescriptionFlags>(subpass.flags));
            HASH(subpass.pipelineBindPoint);

            auto hashReferences{[&hash](const auto &references) {
                HASH(references.size());
                for (const auto &reference : references) {
                    HASH(reference.attachment);
                    HASH(reference.layout);
                }
            }};

            hashReferences(subpass.inputAttachments);
            hashReferences(subpass.colorAttachments);
            hashReferences(subpass.resolveAttachments);

            HASH(subpass.depthStencilAttachment.has_value());
            if (subpass.depthStencilAttachment) {
                HASH(subpass.depthStencilAttachment->attachment);
                HASH(subpass.depthStencilAttachment->layout);
            }

            HASH(subpass.preserveAttachments.size());
            for (const auto &index : subpass.preserveAttachments)
                HASH(index);
        }

        HASH(key.dependencies.size());
        for (const auto &dependency : key.dependencies) {
            HASH(dependency.srcSubpass);
            HASH(dependency.dstSubpass);
            HASH(static_cast<VkDependencyFlags>(dependency.dependencyFlags));
            HASH(static_cast<VkPipelineStageFlags>(dependency.srcStageMask));
            HASH(static_cast<VkPipelineStageFlags>(dependency.dstStageMask));
            HASH(static_cast<VkAccessFlags>(dependency.srcAccessMask));
            HASH(static_cast<VkAccessFlags>(dependency.dstAccessMask));
        }

        return hash;
    }

    size_t RenderPassCache::RenderPassHash::operator()(const vk::RenderPassCreateInfo &key) const {
        size_t hash{};

        HASH(key.attachmentCount);
        for (const auto &attachment : span<const vk::AttachmentDescription>{key.pAttachments, key.attachmentCount}) {
            HASH(static_cast<VkAttachmentDescriptionFlags>(attachment.flags));
            HASH(attachment.format);
            HASH(attachment.samples);
            HASH(attachment.loadOp);
            HASH(attachment.storeOp);
            HASH(attachment.stencilLoadOp);
            HASH(attachment.stencilStoreOp);
            HASH(attachment.initialLayout);
            HASH(attachment.finalLayout);
        }

        HASH(key.subpassCount);
        for (const auto &subpass : span<const vk::SubpassDescription>{key.pSubpasses, key.subpassCount}) {
            HASH(static_cast<VkSubpassDescriptionFlags>(subpass.flags));
            HASH(subpass.pipelineBindPoint);

            auto hashReferences{[&hash](const vk::AttachmentReference *data, u32 count) {
                HASH(count);
                for (const auto &reference : span<const vk::AttachmentReference>{data, count}) {
                    HASH(reference.attachment);
                    HASH(reference.layout);
                }
            }};

            hashReferences(subpass.pInputAttachments, subpass.inputAttachmentCount);
            hashReferences(subpass.pColorAttachments, subpass.colorAttachmentCount);
            if (subpass.pResolveAttachments)
                hashReferences(subpass.pResolveAttachments, subpass.colorAttachmentCount);

            HASH(subpass.pDepthStencilAttachment != nullptr);
            if (subpass.pDepthStencilAttachment) {
                HASH(subpass.pDepthStencilAttachment->attachment);
                HASH(subpass.pDepthStencilAttachment->layout);
            }

            HASH(subpass.preserveAttachmentCount);
            for (const auto &index : span<const u32>{subpass.pPreserveAttachments, subpass.preserveAttachmentCount})
                HASH(index);
        }

        HASH(key.dependencyCount);
        for (const auto &dependency : span<const vk::SubpassDependency>{key.pDependencies, key.dependencyCount}) {
            HASH(dependency.srcSubpass);
            HASH(dependency.dstSubpass);
            HASH(static_cast<VkDependencyFlags>(dependency.dependencyFlags));
            HASH(static_cast<VkPipelineStageFlags>(dependency.srcStageMask));
            HASH(static_cast<VkPipelineStageFlags>(dependency.dstStageMask));
            HASH(static_cast<VkAccessFlags>(dependency.srcAccessMask));
            HASH(static_cast<VkAccessFlags>(dependency.dstAccessMask));
        }

        return hash;
    }

    #undef HASH

    bool RenderPassCache::RenderPassEqual::operator()(const RenderPassMetadata &lhs, const RenderPassMetadata &rhs) const {
        return lhs == rhs;
    }

    bool RenderPassCache::RenderPassEqual::operator()(const RenderPassMetadata &lhs, const vk::RenderPassCreateInfo &rhs) const {
        #define RETF(condition) if (condition) { return false; }
        #define RETARRNEQ(array, pointer, count) RETF(!std::equal(array.begin(), array.end(), pointer, pointer + count))

        RETARRNEQ(lhs.attachments, rhs.pAttachments, rhs.attachmentCount)

        RETF(lhs.subpasses.size() != rhs.subpassCount)
        const vk::SubpassDescription *vkSubpass{rhs.pSubpasses};
        for (const auto &subpass : lhs.subpasses) {
            RETF(subpass.flags != vkSubpass->flags)
            RETF(subpass.pipelineBindPoint != vkSubpass->pipelineBindPoint)

            RETARRNEQ(subpass.inputAttachments, vkSubpass->pInputAttachments, vkSubpass->inputAttachmentCount)
            RETARRNEQ(subpass.colorAttachments, vkSubpass->pColorAttachments, vkSubpass->colorAttachmentCount)

            RETF(subpass.resolveAttachments.size() != (vkSubpass->pResolveAttachments ? vkSubpass->colorAttachmentCount : 0))
            if (!subpass.resolveAttachments.empty())
                RETARRNEQ(subpass.resolveAttachments, vkSubpass->pResolveAttachments, vkSubpass->colorAttachmentCount)

            RETF(subpass.depthStencilAttachment.has_value() != (vkSubpass->pDepthStencilAttachment != nullptr))
            if (subpass.depthStencilAttachment)
                RETF(subpass.depthStencilAttachment->attachment != vkSubpass->pDepthStencilAttachment->attachment &&
                    subpass.depthStencilAttachment->layout != vkSubpass->pDepthStencilAttachment->layout)

            RETARRNEQ(subpass.preserveAttachments, vkSubpass->pPreserveAttachments, vkSubpass->preserveAttachmentCount)

            vkSubpass++;
        }

        RETARRNEQ(lhs.dependencies, rhs.pDependencies, rhs.dependencyCount)

        #undef RETF

        return true;
    }

    vk::RenderPass RenderPassCache::GetRenderPass(const vk::RenderPassCreateInfo &createInfo) {
        std::scoped_lock lock{mutex};
        auto it{renderPassCache.find(createInfo)};
        if (it != renderPassCache.end())
            return *it->second;

        auto entryIt{renderPassCache.try_emplace(RenderPassMetadata{createInfo}, gpu.vkDevice, createInfo)};
        return *entryIt.first->second;
    }
}
