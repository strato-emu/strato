// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "texture.h"

namespace skyline::gpu::format {
    using Format = gpu::texture::FormatBase;
    using vkf = vk::Format;
    using vka = vk::ImageAspectFlagBits;
    using swc = gpu::texture::SwizzleChannel;

    constexpr Format R8G8B8A8Unorm{sizeof(u32), vkf::eR8G8B8A8Unorm};
    constexpr Format R5G6B5Unorm{sizeof(u16), vkf::eR5G6B5UnormPack16};
    constexpr Format A2B10G10R10Unorm{sizeof(u32), vkf::eA2B10G10R10UnormPack32};
    constexpr Format A8B8G8R8Srgb{sizeof(u32), vkf::eA8B8G8R8SrgbPack32};
    constexpr Format A8B8G8R8Snorm{sizeof(u32), vkf::eA8B8G8R8SnormPack32};
    constexpr Format R16G16Snorm{sizeof(u32), vkf::eR16G16Snorm};
    constexpr Format R16G16Float{sizeof(u32), vkf::eR16G16Sfloat};
    constexpr Format B10G11R11Float{sizeof(u32), vkf::eB10G11R11UfloatPack32};
    constexpr Format R32Float{sizeof(u32), vkf::eR32Sfloat};
    constexpr Format R8G8Unorm{sizeof(u16), vkf::eR8G8Unorm};
    constexpr Format R8G8Snorm{sizeof(u16), vkf::eR8G8Snorm};
    constexpr Format R16Unorm{sizeof(u16), vkf::eR16Unorm};
    constexpr Format R16Float{sizeof(u16), vkf::eR16Sfloat};
    constexpr Format R8Unorm{sizeof(u8), vkf::eR8Unorm};
    constexpr Format R32B32G32A32Float{sizeof(u32) * 4, vkf::eR32G32B32A32Sfloat, .swizzle = {
        .blue = swc::Green,
        .green = swc::Blue,
    }};
    constexpr Format R16G16B16A16Unorm{sizeof(u16) * 4, vkf::eR16G16B16A16Unorm};
    constexpr Format R16G16B16A16Uint{sizeof(u16) * 4, vkf::eR16G16B16A16Uint};
    constexpr Format R16G16B16A16Float{sizeof(u16) * 4, vkf::eR16G16B16A16Sfloat};

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
            case vk::Format::eA8B8G8R8SnormPack32:
                return A8B8G8R8Snorm;
            case vk::Format::eR16G16Snorm:
                return R16G16Snorm;
            case vk::Format::eR16G16Sfloat:
                return R16G16Float;
            case vk::Format::eB10G11R11UfloatPack32:
                return B10G11R11Float;
            case vk::Format::eR32Sfloat:
                return format::R32Float;
            case vk::Format::eR16Unorm:
                return R16Unorm;
            case vk::Format::eR16Sfloat:
                return R16Float;
            case vk::Format::eR8G8Unorm:
                return R8G8Unorm;
            case vk::Format::eR8G8Snorm:
                return R8G8Snorm;
            case vk::Format::eR8Unorm:
                return R8Unorm;
            case vk::Format::eR16G16B16A16Unorm:
                return R16G16B16A16Unorm;
            case vk::Format::eR16G16B16A16Uint:
                return R16G16B16A16Uint;
            case vk::Format::eR16G16B16A16Sfloat:
                return R16G16B16A16Float;
            default:
                throw exception("Vulkan format not supported: '{}'", vk::to_string(format));
        }
    }
}
