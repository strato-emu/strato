// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "texture_manager.h"

namespace skyline::gpu {
    TextureManager::TextureManager(GPU &gpu) : gpu(gpu) {}

    void TextureManager::lock() {
        mutex.lock();
    }

    void TextureManager::unlock() {
        mutex.unlock();
    }

    bool TextureManager::try_lock() {
        return mutex.try_lock();
    }

    std::shared_ptr<TextureView> TextureManager::FindOrCreate(const GuestTexture &guestTexture, ContextTag tag) {
        auto guestMapping{guestTexture.mappings.front()};

        /*
         * Iterate over all textures that overlap with the first mapping of the guest texture and compare the mappings:
         * 1) All mappings match up perfectly, we check that the rest of the supplied mappings correspond to mappings in the texture
         * 1.1) If they match as well, we check for format/dimensions/tiling config matching the texture and return or move onto (3)
         * 2) Only a contiguous range of mappings match, we check for if the overlap is meaningful with layout math, it can go two ways:
         * 2.1) If there is a meaningful overlap, we check for format/dimensions/tiling config compatibility and return or move onto (3)
         * 2.2) If there isn't, we move onto (3)
         * 3) If there's another overlap we go back to (1) with it else we go to (4)
         * 4) We check all the overlapping texture for if they're in the texture pool:
         * 4.1) If they are, we do nothing to them
         * 4.2) If they aren't, we delete them from the map
         * 5) Create a new texture and insert it in the map then return it
         */

        std::shared_ptr<Texture> match{};
        auto mappingEnd{std::upper_bound(textures.begin(), textures.end(), guestMapping)}, hostMapping{mappingEnd};
        while (hostMapping != textures.begin() && (--hostMapping)->end() > guestMapping.begin()) {
            auto &hostMappings{hostMapping->texture->guest->mappings};
            if (!hostMapping->contains(guestMapping))
                continue;

            // We need to check that all corresponding mappings in the candidate texture and the guest texture match up
            // Only the start of the first matched mapping and the end of the last mapping can not match up as this is the case for views
            auto firstHostMapping{hostMapping->iterator};
            auto lastGuestMapping{guestTexture.mappings.back()};
            auto lastHostMapping{std::find_if(firstHostMapping, hostMappings.end(), [&lastGuestMapping](const span<u8> &it) {
                return lastGuestMapping.begin() > it.begin() && lastGuestMapping.end() > it.end();
            })}; //!< A past-the-end iterator for the last host mapping, the final valid mapping is prior to this iterator
            bool mappingMatch{std::equal(firstHostMapping, lastHostMapping, guestTexture.mappings.begin(), guestTexture.mappings.end(), [](const span<u8> &lhs, const span<u8> &rhs) {
                return lhs.end() == rhs.end(); // We check end() here to implicitly ignore any offset from the first mapping
            })};

            if (firstHostMapping == hostMappings.begin() && firstHostMapping->begin() == guestMapping.begin() && mappingMatch && lastHostMapping == hostMappings.end() && lastGuestMapping.end() == std::prev(lastHostMapping)->end()) {
                // We've gotten a perfect 1:1 match for *all* mappings from the start to end, we just need to check for compatibility aside from this
                auto &matchGuestTexture{*hostMapping->texture->guest};
                if (matchGuestTexture.format->IsCompatible(*guestTexture.format) &&
                     ((matchGuestTexture.dimensions.width == guestTexture.dimensions.width &&
                       matchGuestTexture.dimensions.height == guestTexture.dimensions.height &&
                       matchGuestTexture.dimensions.depth == guestTexture.GetViewDepth())
                      || matchGuestTexture.viewMipBase > 0)
                     && matchGuestTexture.tileConfig == guestTexture.tileConfig) {
                    auto &texture{hostMapping->texture};
                    ContextLock textureLock{tag, *texture};
                    return texture->GetView(guestTexture.viewType, vk::ImageSubresourceRange{
                        .aspectMask = guestTexture.aspect,
                        .baseMipLevel = guestTexture.viewMipBase,
                        .levelCount = guestTexture.viewMipCount,
                        .layerCount = guestTexture.GetViewLayerCount(),
                    }, guestTexture.format, guestTexture.swizzle);
                }
            } /* else if (mappingMatch) {
                // We've gotten a partial match with a certain subset of contiguous mappings matching, we need to check if this is a meaningful overlap
                if (MeaningfulOverlap) {
                    // TODO: Layout Checks + Check match against Base Layer in TIC
                    auto &texture{hostMapping->texture};
                    return TextureView(texture, static_cast<vk::ImageViewType>(guestTexture.type), vk::ImageSubresourceRange{
                        .aspectMask = guestTexture.format->vkAspect,
                        .levelCount = texture->mipLevels,
                        .layerCount = texture->layerCount,
                    }, guestTexture.format);
                }
            } */
        }

        // Create a texture as we cannot find one that matches
        auto texture{std::make_shared<Texture>(gpu, guestTexture)};
        texture->SetupGuestMappings();
        texture->TransitionLayout(vk::ImageLayout::eGeneral);
        auto it{texture->guest->mappings.begin()};
        textures.emplace(mappingEnd, TextureMapping{texture, it, guestMapping});
        while ((++it) != texture->guest->mappings.end()) {
            guestMapping = *it;
            auto mapping{std::upper_bound(textures.begin(), textures.end(), guestMapping)};
            // TODO: Delete overlapping textures that aren't in texture pool
            textures.emplace(mapping, TextureMapping{texture, it, guestMapping});
        }

        return texture->GetView(guestTexture.viewType, vk::ImageSubresourceRange{
            .aspectMask = guestTexture.aspect,
            .baseMipLevel = guestTexture.viewMipBase,
            .levelCount = guestTexture.viewMipCount,
            .layerCount = guestTexture.GetViewLayerCount(),
        }, guestTexture.format, guestTexture.swizzle);
    }
}
