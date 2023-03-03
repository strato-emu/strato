// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "texture.h"

namespace skyline::gpu::format {
    // @fmt:off

    using vka = vk::ImageAspectFlagBits;

    #define FORMAT(name, bitsPerBlock, format, ...) \
            constexpr gpu::texture::FormatBase name{bitsPerBlock / 8, vk::Format::format, ##__VA_ARGS__}

    #define FORMAT_SUFF_UNORM_SRGB(name, bitsPerBlock, format, fmtSuffix, ...) \
            FORMAT(name ## Unorm, bitsPerBlock, format ## Unorm ## fmtSuffix, ##__VA_ARGS__);   \
            FORMAT(name ## Srgb, bitsPerBlock, format ## Srgb ## fmtSuffix, ##__VA_ARGS__)

    #define FORMAT_SUFF_INT(name, bitsPerBlock, format, fmtSuffix, ...) \
            FORMAT(name ## Uint, bitsPerBlock, format ## Uint ## fmtSuffix, ##__VA_ARGS__);   \
            FORMAT(name ## Sint, bitsPerBlock, format ## Sint ## fmtSuffix, ##__VA_ARGS__)

    #define FORMAT_INT(name, bitsPerBlock, format, ...) \
            FORMAT_SUFF_INT(name, bitsPerBlock, format,, ##__VA_ARGS__)

    #define FORMAT_SUFF_INT_FLOAT(name, bitsPerBlock, format, fmtSuffix, ...) \
            FORMAT_SUFF_INT(name, bitsPerBlock, format, fmtSuffix, ##__VA_ARGS__) ; \
            FORMAT(name ## Float, bitsPerBlock, format ## Sfloat ## fmtSuffix, ##__VA_ARGS__)

    #define FORMAT_INT_FLOAT(name, bitsPerBlock, format, ...) \
            FORMAT_SUFF_INT_FLOAT(name, bitsPerBlock, format,, ##__VA_ARGS__)

    #define FORMAT_SUFF_NORM(name, bitsPerBlock, format, fmtSuffix, ...) \
            FORMAT(name ## Unorm, bitsPerBlock, format ## Unorm ## fmtSuffix, ##__VA_ARGS__); \
            FORMAT(name ## Snorm, bitsPerBlock, format ## Snorm ## fmtSuffix, ##__VA_ARGS__)

    #define FORMAT_SUFF_NORM_INT(name, bitsPerBlock, format, fmtSuffix, ...) \
            FORMAT_SUFF_INT(name, bitsPerBlock, format, fmtSuffix, ##__VA_ARGS__); \
            FORMAT_SUFF_NORM(name, bitsPerBlock, format, fmtSuffix, ##__VA_ARGS__)

    #define FORMAT_NORM_INT(name, bitsPerBlock, format, ...) \
            FORMAT_SUFF_NORM_INT(name, bitsPerBlock, format,, ##__VA_ARGS__)

    #define FORMAT_SUFF_NORM_INT_SRGB(name, bitsPerBlock, format, fmtSuffix, ...) \
            FORMAT_SUFF_NORM_INT(name, bitsPerBlock, format, fmtSuffix, ##__VA_ARGS__); \
            FORMAT(name ## Srgb, bitsPerBlock, format ## Srgb ## fmtSuffix, ##__VA_ARGS__)

    #define FORMAT_NORM_INT_SRGB(name, bitsPerBlock, format, ...) \
            FORMAT_SUFF_NORM_INT_SRGB(name, bitsPerBlock, format,, ##__VA_ARGS__)

    #define FORMAT_SUFF_NORM_INT_FLOAT(name, bitsPerBlock, format, fmtSuffix, ...) \
            FORMAT_SUFF_NORM_INT(name, bitsPerBlock, format, fmtSuffix, ##__VA_ARGS__) ; \
            FORMAT(name ## Float, bitsPerBlock, format ## Sfloat ## fmtSuffix, ##__VA_ARGS__)

    #define FORMAT_NORM_INT_FLOAT(name, bitsPerBlock, format, ...) \
            FORMAT_SUFF_NORM_INT_FLOAT(name, bitsPerBlock, format,, ##__VA_ARGS__)

    // These are ordered according to Size -> Component Count -> R/G/B/A/E Order

    // Color formats
    FORMAT_NORM_INT_SRGB(R8, 8, eR8);

    FORMAT_NORM_INT_FLOAT(R16, 16, eR16);
    FORMAT_NORM_INT_SRGB(R8G8, 16, eR8G8);
    FORMAT(B5G6R5Unorm, 16, eB5G6R5UnormPack16);
    FORMAT(R5G6B5Unorm, 16, eR5G6B5UnormPack16);
    FORMAT(R4G4B4A4Unorm, 16, eR4G4B4A4UnormPack16, .swizzleMapping = {
           .r = vk::ComponentSwizzle::eA,
           .g = vk::ComponentSwizzle::eB,
           .b = vk::ComponentSwizzle::eG,
           .a = vk::ComponentSwizzle::eR
    });
    FORMAT(B5G5R5A1Unorm, 16, eB5G5R5A1UnormPack16);
    FORMAT(A1B5G5R5Unorm, 16, eA1R5G5B5UnormPack16, .swizzleMapping = {
           .r = vk::ComponentSwizzle::eB,
           .g = vk::ComponentSwizzle::eG,
           .b = vk::ComponentSwizzle::eR,
           .a = vk::ComponentSwizzle::eA,
    });
    FORMAT(A1R5G5B5Unorm, 16, eA1R5G5B5UnormPack16);

    FORMAT_INT_FLOAT(R32, 32, eR32);
    FORMAT_NORM_INT_FLOAT(R16G16, 32, eR16G16);
    FORMAT(B10G11R11Float, 32, eB10G11R11UfloatPack32);
    FORMAT_NORM_INT_SRGB(R8G8B8A8, 32, eR8G8B8A8);
    FORMAT_NORM_INT_SRGB(B8G8R8A8, 32, eB8G8R8A8);
    FORMAT_SUFF_NORM_INT(A2B10G10R10, 32, eA2B10G10R10, Pack32);
    FORMAT_SUFF_NORM_INT_SRGB(A8B8G8R8, 32, eA8B8G8R8, Pack32);
    FORMAT(E5B9G9R9Float, 32, eE5B9G9R9UfloatPack32);

    FORMAT_INT_FLOAT(R32G32, 32 * 2, eR32G32);
    FORMAT_NORM_INT_FLOAT(R16G16B16, 16 * 3, eR16G16B16);
    FORMAT_NORM_INT_FLOAT(R16G16B16A16, 16 * 4, eR16G16B16A16);

    FORMAT_INT_FLOAT(R32G32B32A32, 32 * 4, eR32G32B32A32);

    // Compressed Colour Formats
    FORMAT_SUFF_UNORM_SRGB(BC1, 64, eBc1Rgba, Block,
                           .blockWidth = 4,
                           .blockHeight = 4
    );
    FORMAT_SUFF_NORM(BC4, 64, eBc4, Block,
                     .blockWidth = 4,
                     .blockHeight = 4,
    );
    FORMAT_SUFF_UNORM_SRGB(BC2, 128, eBc2, Block,
                           .blockWidth = 4,
                           .blockHeight = 4
    );
    FORMAT_SUFF_UNORM_SRGB(BC3, 128, eBc3, Block,
                           .blockWidth = 4,
                           .blockHeight = 4
    );
    FORMAT_SUFF_NORM(BC5, 128, eBc5, Block,
                     .blockWidth = 4,
                     .blockHeight = 4,
    );
    FORMAT(Bc6HUfloat, 128, eBc6HUfloatBlock,
           .blockWidth = 4,
           .blockHeight = 4,
    );
    FORMAT(Bc6HSfloat, 128, eBc6HSfloatBlock,
           .blockWidth = 4,
           .blockHeight = 4,
    );
    FORMAT_SUFF_UNORM_SRGB(BC7, 128, eBc7, Block,
                           .blockWidth = 4,
                           .blockHeight = 4
    );

    FORMAT_SUFF_UNORM_SRGB(Astc4x4, 128, eAstc4x4, Block,
                           .blockWidth = 4,
                           .blockHeight = 4
    );
    FORMAT_SUFF_UNORM_SRGB(Astc5x4, 128, eAstc5x4, Block,
                           .blockWidth = 5,
                           .blockHeight = 4
    );
    FORMAT_SUFF_UNORM_SRGB(Astc5x5, 128, eAstc5x5, Block,
                           .blockWidth = 5,
                           .blockHeight = 5
    );
    FORMAT_SUFF_UNORM_SRGB(Astc6x5, 128, eAstc6x5, Block,
                           .blockWidth = 6,
                           .blockHeight = 5
    );
    FORMAT_SUFF_UNORM_SRGB(Astc6x6, 128, eAstc6x6, Block,
                           .blockWidth = 6,
                           .blockHeight = 6
    );
    FORMAT_SUFF_UNORM_SRGB(Astc8x5, 128, eAstc8x5, Block,
                           .blockWidth = 8,
                           .blockHeight = 5
    );
    FORMAT_SUFF_UNORM_SRGB(Astc8x6, 128, eAstc8x6, Block,
                           .blockWidth = 8,
                           .blockHeight = 6
    );
    FORMAT_SUFF_UNORM_SRGB(Astc8x8, 128, eAstc8x8, Block,
                           .blockWidth = 8,
                           .blockHeight = 8
    );
    FORMAT_SUFF_UNORM_SRGB(Astc10x5, 128, eAstc10x5, Block,
                           .blockWidth = 10,
                           .blockHeight = 5
    );
    FORMAT_SUFF_UNORM_SRGB(Astc10x6, 128, eAstc10x6, Block,
                           .blockWidth = 10,
                           .blockHeight = 6
    );
    FORMAT_SUFF_UNORM_SRGB(Astc10x8, 128, eAstc10x8, Block,
                           .blockWidth = 10,
                           .blockHeight = 8
    );
    FORMAT_SUFF_UNORM_SRGB(Astc10x10, 128, eAstc10x10, Block,
                           .blockWidth = 10,
                           .blockHeight = 10
    );
    FORMAT_SUFF_UNORM_SRGB(Astc12x10, 128, eAstc12x10, Block,
                           .blockWidth = 12,
                           .blockHeight = 10
    );
    FORMAT_SUFF_UNORM_SRGB(Astc12x12, 128, eAstc12x12, Block,
                           .blockWidth = 12,
                           .blockHeight = 12
    );

    // Depth/Stencil Formats
    // All of these have a G->R swizzle
    FORMAT(D16Unorm, 16, eD16Unorm, vka::eDepth, .swizzleMapping = {
           .r = vk::ComponentSwizzle::eR,
           .g = vk::ComponentSwizzle::eR,
           .b = vk::ComponentSwizzle::eB,
           .a = vk::ComponentSwizzle::eA,
    });
    FORMAT(D32Float, 32, eD32Sfloat, vka::eDepth, .swizzleMapping = {
           .r = vk::ComponentSwizzle::eR,
           .g = vk::ComponentSwizzle::eR,
           .b = vk::ComponentSwizzle::eB,
           .a = vk::ComponentSwizzle::eA,
    });
    FORMAT(D24UnormX8Uint, 32, eX8D24UnormPack32, .vkAspect = {
           vka::eDepth
    }, .swizzleMapping = {
           .r = vk::ComponentSwizzle::eR,
           .g = vk::ComponentSwizzle::eR,
           .b = vk::ComponentSwizzle::eB,
           .a = vk::ComponentSwizzle::eA,
    });
    FORMAT(D24UnormS8Uint, 32, eD24UnormS8Uint, .vkAspect = {
           vka::eStencil | vka::eDepth
    }, .swizzleMapping = {
           .r = vk::ComponentSwizzle::eR,
           .g = vk::ComponentSwizzle::eR,
           .b = vk::ComponentSwizzle::eB,
           .a = vk::ComponentSwizzle::eA,
    });
    FORMAT(D32FloatS8Uint, 32, eD32SfloatS8Uint, .vkAspect = {
           vka::eStencil | vka::eDepth
    }, .swizzleMapping = {
           .r = vk::ComponentSwizzle::eR,
           .g = vk::ComponentSwizzle::eR,
           .b = vk::ComponentSwizzle::eB,
           .a = vk::ComponentSwizzle::eA,
    });
    FORMAT(S8UintD24Unorm, 32, eD24UnormS8Uint, .vkAspect = {
            vka::eStencil | vka::eDepth
    }, .swizzleMapping = {
            .r = vk::ComponentSwizzle::eR,
            .g = vk::ComponentSwizzle::eR,
            .b = vk::ComponentSwizzle::eB,
            .a = vk::ComponentSwizzle::eA,
    }, true);
    FORMAT(S8Uint, 32, eS8Uint, .vkAspect = {
           vka::eStencil
    }, .swizzleMapping = {
           .r = vk::ComponentSwizzle::eR,
           .g = vk::ComponentSwizzle::eR,
           .b = vk::ComponentSwizzle::eB,
           .a = vk::ComponentSwizzle::eA,
    });

    #undef FORMAT
    #undef FORMAT_SUFF_UNORM_SRGB
    #undef FORMAT_SUFF_INT
    #undef FORMAT_INT
    #undef FORMAT_SUFF_INT_FLOAT
    #undef FORMAT_INT_FLOAT
    #undef FORMAT_SUFF_NORM_INT
    #undef FORMAT_NORM_INT
    #undef FORMAT_SUFF_NORM_INT_SRGB
    #undef FORMAT_NORM_INT_SRGB
    #undef FORMAT_SUFF_NORM_INT_FLOAT
    #undef FORMAT_NORM_INT_FLOAT

    // @fmt:on
}
