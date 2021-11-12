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
    constexpr Format R16G16Unorm{sizeof(u32), vkf::eR16G16Unorm};
    constexpr Format R16G16Snorm{sizeof(u32), vkf::eR16G16Snorm};
    constexpr Format R16G16Sint{sizeof(u32), vkf::eR16G16Sint};
    constexpr Format R16G16Uint{sizeof(u32), vkf::eR16G16Uint};
    constexpr Format R16G16Float{sizeof(u32), vkf::eR16G16Sfloat};
    constexpr Format B10G11R11Float{sizeof(u32), vkf::eB10G11R11UfloatPack32};
    constexpr Format R32Float{sizeof(u32), vkf::eR32Sfloat};
    constexpr Format R8G8Unorm{sizeof(u16), vkf::eR8G8Unorm};
    constexpr Format R8G8Snorm{sizeof(u16), vkf::eR8G8Snorm};
    constexpr Format R16Unorm{sizeof(u16), vkf::eR16Unorm};
    constexpr Format R16Float{sizeof(u16), vkf::eR16Sfloat};
    constexpr Format R8Unorm{sizeof(u8), vkf::eR8Unorm};
    constexpr Format R8Snorm{sizeof(u8), vkf::eR8Snorm};
    constexpr Format R8Sint{sizeof(u8), vkf::eR8Sint};
    constexpr Format R8Uint{sizeof(u8), vkf::eR8Uint};
    constexpr Format R32B32G32A32Float{sizeof(u32) * 4, vkf::eR32G32B32A32Sfloat, .swizzle = {
        .blue = swc::Green,
        .green = swc::Blue,
    }};
    constexpr Format R16G16B16A16Unorm{sizeof(u16) * 4, vkf::eR16G16B16A16Unorm};
    constexpr Format R16G16B16A16Snorm{sizeof(u16) * 4, vkf::eR16G16B16A16Snorm};
    constexpr Format R16G16B16A16Sint{sizeof(u16) * 4, vkf::eR16G16B16A16Sint};
    constexpr Format R16G16B16A16Uint{sizeof(u16) * 4, vkf::eR16G16B16A16Uint};
    constexpr Format R16G16B16A16Float{sizeof(u16) * 4, vkf::eR16G16B16A16Sfloat};

    /**
}
