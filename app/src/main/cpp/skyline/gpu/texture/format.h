// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "texture.h"

namespace skyline::gpu::format {
    // @fmt:off

    using vka = vk::ImageAspectFlagBits;
    using swc = gpu::texture::SwizzleChannel;

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

    // These are ordered according to Size -> Component Count -> R/G/B/A Order

    // Color formats
    FORMAT_NORM_INT_SRGB(R8, 8, eR8);
    FORMAT_NORM_INT_SRGB(R8R001, 8, eR8);

    FORMAT_NORM_INT_FLOAT(R16, 16, eR16);
    FORMAT_NORM_INT_SRGB(R8G8, 16, eR8G8);
    FORMAT(R5G6B5Unorm, 16, eR5G6B5UnormPack16);
    FORMAT(B5G6R5Unorm, 16, eB5G6R5UnormPack16);
    FORMAT(B5G5R5A1Unorm, 16, eB5G5R5A1UnormPack16);
    FORMAT_INT_FLOAT(R32, 32, eR32);
    FORMAT_NORM_INT_FLOAT(R16G16, 32, eR16G16);
    FORMAT(B10G11R11Float, 32, eB10G11R11UfloatPack32);
    FORMAT_NORM_INT_SRGB(R8G8B8A8, 32, eR8G8B8A8);
    FORMAT_NORM_INT_SRGB(G8B8A8R8, 32, eB8G8R8A8, .swizzle = {
        .blue = swc::Green,
        .green = swc::Blue,
        .red = swc::Alpha,
        .alpha = swc::Red
    });
    FORMAT_NORM_INT_SRGB(B8G8R8A8, 32, eB8G8R8A8);
    FORMAT_SUFF_NORM_INT(A2B10G10R10, 32, eA2B10G10R10, Pack32);
    FORMAT_SUFF_NORM_INT(A2R10G10B10, 32, eA2B10G10R10, Pack32, .swizzle = {
        .blue = swc::Red,
        .red = swc::Blue
    });

    FORMAT_SUFF_NORM_INT_SRGB(A8B8G8R8, 32, eA8B8G8R8, Pack32);
    FORMAT_INT_FLOAT(R32G32, 32 * 2, eR32G32);
    FORMAT_NORM_INT_FLOAT(R16G16B16A16, 16 * 4, eR16G16B16A16);
    FORMAT_INT_FLOAT(R32G32B32A32, 32 * 4, eR32G32B32A32);
    FORMAT_INT_FLOAT(R32B32G32A32, 32 * 4, eR32G32B32A32, .swizzle = {
        .blue = swc::Green,
        .green = swc::Blue,
    });

    // Compressed Colour Formats
    FORMAT_SUFF_UNORM_SRGB(Bc1, 64, eBc1Rgba, Block,
        .blockWidth = 4,
        .blockHeight = 4
    );

    FORMAT_SUFF_NORM(Bc4111R, 64, eBc4, Block,
       .blockWidth = 4,
       .blockHeight = 4,
       .swizzle = {
           .red = swc::One,
           .green = swc::One,
           .blue = swc::One,
           .alpha = swc::Red
       }
    );

    FORMAT_SUFF_NORM(Bc4RRR1, 64, eBc4, Block,
         .blockWidth = 4,
         .blockHeight = 4,
         .swizzle = {
             .red = swc::Red,
             .green = swc::Red,
             .blue = swc::Red,
             .alpha = swc::One
         }
    );

    FORMAT_SUFF_UNORM_SRGB(Astc4x4, 128, eAstc4x4, Block,
        .blockWidth = 4,
        .blockHeight = 4
    );

    // Depth/Stencil Formats
    FORMAT(D32Float, 32, eD32Sfloat, vka::eDepth);
    FORMAT(S8D24Unorm, 32, eD24UnormS8Uint, .vkAspect = {
        vka::eStencil | vka::eDepth
    }); // TODO: Swizzle Depth/Stencil

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
