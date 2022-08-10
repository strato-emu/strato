// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#pragma once

#include <common.h>
#include <soc/gm20b/engines/engine.h>

namespace skyline::soc::gm20b::engine::fermi2d::type {
    #pragma pack(push, 1)

    enum class MemoryLayout {
        BlockLinear = 0,
        Pitch = 1
    };

    struct Surface {
        enum class SurfaceFormat {
            Y1_8X8 = 0x1C,
            AY8 = 0x1D,
            R32G32B32A32Float = 0xC0,
            R32G32B32X32Float = 0xC3,
            R16G16B16X16Unorm = 0xC6,
            R16G16B16X16Snorm = 0xC7,
            R16G16B16A16Float = 0xCA,
            R32G32Float = 0xCB,
            R16G16B16X16Float = 0xCE,
            B8G8R8A8Unorm = 0xCF,
            B8G8R8A8Srgb = 0xD0,
            A2B10G10R10Unorm = 0xD1,
            R8G8B8A8Unorm = 0xD5,
            R8G8B8A8Srgb = 0xD6,
            R8G8B8X8Snorm = 0xD7,
            R16G16Unorm = 0xDA,
            R16G16Snorm = 0xDB,
            R16G16Float = 0xDE,
            A2R10G10B10 = 0xDF,
            B10G11R11Float = 0xE0,
            R32Float = 0xE5,
            B8G8R8X8Unorm = 0xE6,
            B8G8R8X8Srgb = 0xE7,
            B5G6R5Unorm = 0xE8,
            B5G5R5A1Unorm = 0xE9,
            R8G8Unorm = 0xEA,
            R8G8Snorm = 0xEB,
            R16Unorm = 0xEE,
            R16Snorm = 0xEF,
            R16Float = 0xF2,
            R8Unorm = 0xF3,
            R8Snorm = 0xF4,
            A8 = 0xF7,
            B5G5R5X1Unorm = 0xF8,
            R8G8B8X8Unorm = 0xF9,
            R8G8B8X8Srgb = 0xFA,
            Z1R5G5B5 = 0xFB,
            O1R5G5B5 = 0xFC,
            Z8R8G8B8 = 0xFD,
            O8R8G8B8 = 0xFE,
            Y32 = 0xFF
        } format;

        MemoryLayout memoryLayout;

        struct {
            u8 widthLog2 : 4;
            u8 heightLog2 : 4;
            u8 depthLog2 : 4;
            u32 _pad_ : 20;

            u8 Width() const {
                return static_cast<u8>(1 << widthLog2);
            }

            u8 Height() const {
                return static_cast<u8>(1 << heightLog2);
            }

            u8 Depth() const {
                return static_cast<u8>(1 << depthLog2);
            }
        } blockSize;

        u32 depth;
        u32 layer;
        u32 stride;
        u32 width;
        u32 height;
        Address address;
    };

    enum class SampleModeOrigin : u8 {
        Center = 0,
        Corner = 1
    };

    enum class SampleModeFilter : u8 {
        Point = 0,
        Bilinear = 1
    };

    #pragma pack(pop)
}
