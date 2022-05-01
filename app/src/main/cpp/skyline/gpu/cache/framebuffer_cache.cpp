// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <boost/functional/hash.hpp>
#include <gpu.h>
#include "framebuffer_cache.h"

namespace skyline::gpu::cache {
    FramebufferCache::FramebufferCache(GPU &gpu) : gpu(gpu) {}

    FramebufferCache::FramebufferImagelessAttachment::FramebufferImagelessAttachment(const vk::FramebufferAttachmentImageInfo &info) : flags(info.flags), usage(info.usage), width(info.width), height(info.height), layers(info.layerCount), format(*info.pViewFormats) {}

    FramebufferCache::FramebufferCacheKey::FramebufferCacheKey(const FramebufferCreateInfo &createInfo) {
        auto &info{createInfo.get<vk::FramebufferCreateInfo>()};
        flags = info.flags;
        renderPass = info.renderPass;
        width = info.width;
        height = info.height;
        layers = info.layers;

        if (createInfo.isLinked<vk::FramebufferAttachmentsCreateInfo>()) {
            auto &attachmentInfo{createInfo.get<vk::FramebufferAttachmentsCreateInfo>()};
            std::vector<FramebufferImagelessAttachment> imagelessAttachments;
            imagelessAttachments.reserve(attachmentInfo.attachmentImageInfoCount);
            for (const auto &image : span<const vk::FramebufferAttachmentImageInfo>(attachmentInfo.pAttachmentImageInfos, attachmentInfo.attachmentImageInfoCount))
                imagelessAttachments.emplace_back(image);
            attachments.emplace<std::vector<FramebufferImagelessAttachment>>(std::move(imagelessAttachments));
        } else {
            std::vector<vk::ImageView> imageViews;
            imageViews.reserve(info.attachmentCount);
            for (const auto &image : span<const vk::ImageView>(info.pAttachments, info.attachmentCount))
                imageViews.emplace_back(image);
            attachments.emplace<std::vector<vk::ImageView>>(std::move(imageViews));
        }
    }

    #define HASH(x) boost::hash_combine(hash, x)

    size_t FramebufferCache::FramebufferHash::operator()(const FramebufferCacheKey &key) const {
        size_t hash{};

        HASH(static_cast<VkFramebufferCreateFlags>(key.flags));
        HASH(static_cast<VkRenderPass>(key.renderPass));
        HASH(key.width);
        HASH(key.height);
        HASH(key.layers);

        std::visit(VariantVisitor{
            [&hash](const std::vector<FramebufferImagelessAttachment> &attachments) {
                HASH(attachments.size());
                for (const auto &attachment : attachments) {
                    HASH(static_cast<VkImageCreateFlags>(attachment.flags));
                    HASH(static_cast<VkImageUsageFlags>(attachment.usage));
                    HASH(attachment.width);
                    HASH(attachment.height);
                    HASH(attachment.layers);
                    HASH(attachment.format);
                }
            },
            [&hash](const std::vector<vk::ImageView> &attachments) {
                HASH(attachments.size());
                for (const auto &attachment : attachments)
                    HASH(static_cast<VkImageView>(attachment));
            }
        }, key.attachments);

        return hash;
    }

    size_t FramebufferCache::FramebufferHash::operator()(const FramebufferCreateInfo &key) const {
        size_t hash{};

        auto &info{key.get<vk::FramebufferCreateInfo>()};

        HASH(static_cast<VkFramebufferCreateFlags>(info.flags));
        HASH(static_cast<VkRenderPass>(info.renderPass));
        HASH(info.width);
        HASH(info.height);
        HASH(info.layers);

        if (info.flags & vk::FramebufferCreateFlagBits::eImageless) {
            auto &attachmentInfo{key.get<vk::FramebufferAttachmentsCreateInfo>()};
            for (const vk::FramebufferAttachmentImageInfo &image : span<const vk::FramebufferAttachmentImageInfo>(attachmentInfo.pAttachmentImageInfos, attachmentInfo.attachmentImageInfoCount)) {
                HASH(static_cast<VkImageCreateFlags>(image.flags));
                HASH(static_cast<VkImageUsageFlags>(image.usage));
                HASH(image.width);
                HASH(image.height);
                HASH(image.layerCount);
                HASH(*image.pViewFormats);
            }
        } else {
            HASH(info.attachmentCount);
            for (const auto &view : span<const vk::ImageView>(info.pAttachments, info.attachmentCount))
                HASH(static_cast<VkImageView>(view));
        }

        return hash;
    }

    #undef HASH

    bool FramebufferCache::FramebufferEqual::operator()(const FramebufferCacheKey &lhs, const FramebufferCacheKey &rhs) const {
        return lhs == rhs;
    }

    bool FramebufferCache::FramebufferEqual::operator()(const FramebufferCacheKey &lhs, const FramebufferCreateInfo &rhs) const {
        #define RETF(condition) if (condition) { return false; }

        auto &rhsInfo{rhs.get<vk::FramebufferCreateInfo>()};

        RETF(lhs.flags != rhsInfo.flags)
        RETF(lhs.renderPass != rhsInfo.renderPass)
        RETF(lhs.width != rhsInfo.width)
        RETF(lhs.height != rhsInfo.height)
        RETF(lhs.layers != rhsInfo.layers)

        if (lhs.flags & vk::FramebufferCreateFlagBits::eImageless) {
            auto &lhsAttachments{std::get<std::vector<FramebufferImagelessAttachment>>(lhs.attachments)};
            auto &rhsAttachments{rhs.get<vk::FramebufferAttachmentsCreateInfo>()};

            RETF(lhsAttachments.size() != rhsAttachments.attachmentImageInfoCount)
            const vk::FramebufferAttachmentImageInfo *rhsAttachmentInfo{rhsAttachments.pAttachmentImageInfos};
            for (const auto &attachment : lhsAttachments) {
                RETF(attachment.flags != rhsAttachmentInfo->flags)
                RETF(attachment.usage != rhsAttachmentInfo->usage)
                RETF(attachment.width != rhsAttachmentInfo->width)
                RETF(attachment.height != rhsAttachmentInfo->height)
                RETF(attachment.layers != rhsAttachmentInfo->layerCount)
                RETF(attachment.format != *rhsAttachmentInfo->pViewFormats)
                rhsAttachmentInfo++;
            }
        } else {
            auto &lhsAttachments{std::get<std::vector<vk::ImageView>>(lhs.attachments)};
            span<const vk::ImageView> rhsAttachments{rhsInfo.pAttachments, rhsInfo.attachmentCount};
            RETF(!std::equal(lhsAttachments.begin(), lhsAttachments.end(), rhsAttachments.begin(), rhsAttachments.end()))
        }

        #undef RETF

        return true;
    }

    vk::Framebuffer FramebufferCache::GetFramebuffer(const FramebufferCreateInfo &createInfo) {
        std::scoped_lock lock{mutex};
        auto it{framebufferCache.find(createInfo)};
        if (it != framebufferCache.end())
            return *it->second;

        auto entryIt{framebufferCache.try_emplace(FramebufferCacheKey{createInfo}, gpu.vkDevice, createInfo.get<vk::FramebufferCreateInfo>())};
        return *entryIt.first->second;
    }
}
