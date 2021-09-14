// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "texture.h"

namespace skyline::gpu::format {
    using Format = gpu::texture::FormatBase;
    using vkf = vk::Format;
    using vka = vk::ImageAspectFlagBits;

    constexpr Format R8G8B8A8Unorm{sizeof(u32), 1, 1, vkf::eR8G8B8A8Unorm, vka::eColor};
    constexpr Format R5G6B5Unorm{sizeof(u16), 1, 1, vkf::eR5G6B5UnormPack16, vka::eColor};
    constexpr Format A2B10G10R10Unorm{sizeof(u32), 1, 1, vkf::eA2B10G10R10UnormPack32, vka::eColor};
    constexpr Format A8B8G8R8Srgb{sizeof(u32), 1, 1, vkf::eA8B8G8R8SrgbPack32, vka::eColor};

    /**
     * @brief Converts a Vulkan format to a Skyline format
     */
    constexpr gpu::texture::Format GetFormat(vk::Format format) {
        switch (format) {
            case vk::Format::eR8G8B8A8Unorm:
                return R8G8B8A8Unorm;
            case vk::Format::eR5G6B5UnormPack16:
                return R5G6B5Unorm;
            case vk::Format::eA2B10G10R10UnormPack32:
                return A2B10G10R10Unorm;
            case vk::Format::eA8B8G8R8SrgbPack32:
                return A8B8G8R8Srgb;
            default:
                throw exception("Vulkan format not supported: '{}'", vk::to_string(format));
        }
    }
}
