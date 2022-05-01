// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <boost/functional/hash.hpp>
#include <gpu.h>
#include "renderpass_cache.h"

namespace skyline::gpu::cache {
    RenderPassCache::RenderPassCache(gpu::GPU &gpu) : gpu(gpu) {}

    RenderPassCache::RenderPassMetadata::RenderPassMetadata(const vk::RenderPassCreateInfo &createInfo) {
        for (const auto &attachment : span<const vk::AttachmentDescription>{createInfo.pAttachments, createInfo.attachmentCount})
            attachments.emplace_back(attachment.format, attachment.samples);

        subpasses.reserve(createInfo.subpassCount);
        for (const auto &subpass : span<const vk::SubpassDescription>{createInfo.pSubpasses, createInfo.subpassCount}) {
            auto &subpassMetadata{subpasses.emplace_back()};

            subpassMetadata.inputAttachments.reserve(subpass.inputAttachmentCount);
            for (const auto &reference : span<const vk::AttachmentReference>{subpass.pInputAttachments, subpass.inputAttachmentCount})
                subpassMetadata.inputAttachments.emplace_back(reference.attachment);

            subpassMetadata.colorAttachments.reserve(subpass.colorAttachmentCount);
            for (const auto &reference : span<const vk::AttachmentReference>{subpass.pColorAttachments, subpass.colorAttachmentCount})
                subpassMetadata.colorAttachments.emplace_back(reference.attachment);

            auto resolveAttachmentCount{subpass.pResolveAttachments ? subpass.colorAttachmentCount : 0};
            subpassMetadata.resolveAttachments.reserve(resolveAttachmentCount);
            for (const auto &reference : span<const vk::AttachmentReference>{subpass.pResolveAttachments, resolveAttachmentCount})
                subpassMetadata.resolveAttachments.emplace_back(reference.attachment);

            if (subpass.pDepthStencilAttachment)
                subpassMetadata.depthStencilAttachment.emplace(subpass.pDepthStencilAttachment->attachment);

            subpassMetadata.preserveAttachments.reserve(subpass.preserveAttachmentCount);
            for (const auto &index : span<const u32>{subpass.pPreserveAttachments, subpass.preserveAttachmentCount})
                subpassMetadata.resolveAttachments.emplace_back(index);
        }
    }

    #define HASH(x) boost::hash_combine(hash, x)

    size_t RenderPassCache::RenderPassHash::operator()(const RenderPassMetadata &key) const {
        size_t hash{};

        HASH(key.attachments.size());
        for (const auto &attachment : key.attachments) {
            HASH(attachment.format);
            HASH(attachment.sampleCount);
        }

        HASH(key.subpasses.size());
        for (const auto &subpass : key.subpasses) {
            HASH(subpass.inputAttachments.size());
            for (const auto &reference : subpass.inputAttachments)
                HASH(reference);

            HASH(subpass.colorAttachments.size());
            for (const auto &reference : subpass.colorAttachments)
                HASH(reference);

            HASH(subpass.resolveAttachments.size());
            for (const auto &reference : subpass.resolveAttachments)
                HASH(reference);

            HASH(subpass.depthStencilAttachment.has_value());
            if (subpass.depthStencilAttachment)
                HASH(*subpass.depthStencilAttachment);

            HASH(subpass.preserveAttachments.size());
            for (const auto &index : subpass.preserveAttachments)
                HASH(index);
        }

        return hash;
    }

    size_t RenderPassCache::RenderPassHash::operator()(const vk::RenderPassCreateInfo &key) const {
        size_t hash{};

        HASH(key.attachmentCount);
        for (const auto &attachment : span<const vk::AttachmentDescription>{key.pAttachments, key.attachmentCount}) {
            HASH(attachment.format);
            HASH(attachment.samples);
        }

        HASH(key.subpassCount);
        for (const auto &subpass : span<const vk::SubpassDescription>{key.pSubpasses, key.subpassCount}) {
            HASH(subpass.inputAttachmentCount);
            for (const auto &reference : span<const vk::AttachmentReference>{subpass.pInputAttachments, subpass.inputAttachmentCount})
                HASH(reference.attachment);

            HASH(subpass.colorAttachmentCount);
            for (const auto &reference : span<const vk::AttachmentReference>{subpass.pColorAttachments, subpass.colorAttachmentCount})
                HASH(reference.attachment);

            u32 resolveAttachmentCount{subpass.pResolveAttachments ? subpass.colorAttachmentCount : 0};
            HASH(resolveAttachmentCount);
            for (const auto &reference : span<const vk::AttachmentReference>{subpass.pResolveAttachments, resolveAttachmentCount})
                HASH(reference.attachment);

            HASH(subpass.pDepthStencilAttachment != nullptr);
            if (subpass.pDepthStencilAttachment)
                HASH(subpass.pDepthStencilAttachment->attachment);

            HASH(subpass.preserveAttachmentCount);
            for (const auto &index : span<const u32>{subpass.pPreserveAttachments, subpass.preserveAttachmentCount})
                HASH(index);
        }

        return hash;
    }

    #undef HASH

    bool RenderPassCache::RenderPassEqual::operator()(const RenderPassMetadata &lhs, const RenderPassMetadata &rhs) const {
        return lhs == rhs;
    }

    bool RenderPassCache::RenderPassEqual::operator()(const RenderPassMetadata &lhs, const vk::RenderPassCreateInfo &rhs) const {
        #define RETF(condition) if (condition) { return false; }

        RETF(lhs.attachments.size() != rhs.attachmentCount)
        const vk::AttachmentDescription *vkAttachment{rhs.pAttachments};
        for (const auto &attachment : lhs.attachments) {
            RETF(attachment.format != vkAttachment->format)
            RETF(attachment.sampleCount != vkAttachment->samples)
            vkAttachment++;
        }

        RETF(lhs.subpasses.size() != rhs.subpassCount)
        const vk::SubpassDescription *vkSubpass{rhs.pSubpasses};
        for (const auto &subpass : lhs.subpasses) {
            RETF(subpass.inputAttachments.size() != vkSubpass->inputAttachmentCount)
            const vk::AttachmentReference *vkReference{vkSubpass->pInputAttachments};
            for (const auto &reference : subpass.inputAttachments)
                RETF(reference != (vkReference++)->attachment)

            RETF(subpass.colorAttachments.size() != vkSubpass->colorAttachmentCount)
            vkReference = vkSubpass->pColorAttachments;
            for (const auto &reference : subpass.colorAttachments)
                RETF(reference != (vkReference++)->attachment)

            RETF(subpass.resolveAttachments.size() != (vkSubpass->pResolveAttachments ? vkSubpass->colorAttachmentCount : 0))
            vkReference = vkSubpass->pResolveAttachments;
            for (const auto &reference : subpass.resolveAttachments)
                RETF(reference != (vkReference++)->attachment)

            RETF(subpass.depthStencilAttachment.has_value() != (vkSubpass->pDepthStencilAttachment != nullptr))
            if (subpass.depthStencilAttachment)
                RETF(*subpass.depthStencilAttachment != vkSubpass->pDepthStencilAttachment->attachment)

            RETF(subpass.preserveAttachments.size() != vkSubpass->preserveAttachmentCount)
            const u32 *vkIndex{vkSubpass->pPreserveAttachments};
            for (const auto &attachment : subpass.preserveAttachments)
                RETF(attachment != *(vkIndex++))

            vkSubpass++;
        }

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
