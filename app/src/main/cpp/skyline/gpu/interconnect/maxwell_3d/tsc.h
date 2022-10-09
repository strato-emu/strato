// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#pragma once

#include <common/base.h>

namespace skyline::gpu::interconnect {
    #pragma pack(push, 1)

    /**
     * @brief The Texture Sampler Control is a descriptor used to configure the texture sampler in Maxwell GPUs
     * @url https://github.com/envytools/envytools/blob/master/rnndb/graph/g80_texture.xml#L367
     * @url https://github.com/devkitPro/deko3d/blob/00c12d1f4809014f1cc22719dd2e3476735eec64/source/maxwell/texture_sampler_control_block.h
     */
    struct TextureSamplerControl {
        enum class AddressMode : u32 {
            Repeat = 0,
            MirroredRepeat = 1,
            ClampToEdge = 2,
            ClampToBorder = 3,
            Clamp = 4,
            MirrorClampToEdge = 5,
            MirrorClampToBorder = 6,
            MirrorClamp = 7,
        };

        enum class CompareOp : u32 {
            Never = 0,
            Less = 1,
            Equal = 2,
            LessOrEqual = 3,
            Greater = 4,
            NotEqual = 5,
            GreaterOrEqual = 6,
            Always = 7,
        };

        enum class Filter : u32 {
            Nearest = 1,
            Linear = 2,
        };

        enum class MipFilter : u32 {
            None = 1,
            Nearest = 2,
            Linear = 3,
        };

        enum class SamplerReduction : u32 {
            WeightedAverage = 0,
            Min = 1,
            Max = 2,
        };

        // 0x00
        AddressMode addressModeU : 3;
        AddressMode addressModeV : 3;
        AddressMode addressModeP : 3;
        u32 depthCompareEnable : 1;
        CompareOp depthCompareOp : 3;
        u32 srgbConversion : 1;
        u32 fontFilterWidth : 3;
        u32 fontFilterHeight : 3;
        u32 maxAnisotropy : 3;
        u32 _pad0_ : 9;

        // 0x04
        Filter magFilter : 2;
        u32 _pad1_ : 2;
        Filter minFilter : 2;
        MipFilter mipFilter : 2;
        u32 cubemapAnisotropy : 1;
        u32 cubemapInterfaceFiltering : 1;
        SamplerReduction reductionFilter : 2;
        i32 mipLodBias : 13;
        u32 floatCoordNormalization : 1;
        u32 trilinearOptimization : 5;
        u32 _pad2_ : 1;

        // 0x08
        u32 minLodClamp : 12;
        u32 maxLodClamp : 12;
        u32 srgbBorderColorR : 8;

        // 0x0C
        u32 _pad3_ : 12;
        u32 srgbBorderColorG : 8;
        u32 srgbBorderColorB : 8;
        u32 _pad4_ : 4;

        // 0x10
        float borderColorR;

        // 0x14
        float borderColorG;

        // 0x18
        float borderColorB;

        // 0x1C
        float borderColorA;

      private:
        /**
         * @brief Convert a fixed point integer to a floating point integer
         */
        template<typename T, size_t FractionalBits = 8>
        float ConvertFixedToFloat(T fixed) {
            return static_cast<float>(fixed) / static_cast<float>(1 << FractionalBits);
        };

      public:
        bool operator==(const TextureSamplerControl &) const = default;

        float MaxAnisotropy() {
            constexpr size_t AnisotropyCount{8}; //!< The amount of unique anisotropy values that can be represented (2^3 — 3-bit value)
            constexpr std::array<float, AnisotropyCount> anisotropyLut{
                1.0f, 3.14f, 5.28f, 7.42f, 9.57f, 11.71f, 13.85f, 16.0f
            }; //!< A linear mapping of value range (0..7) to anisotropy range (1..16) calculated using `(index * 15 / 7) + 1`
            return anisotropyLut[maxAnisotropy];
        }

        float MipLodBias() {
            return ConvertFixedToFloat(mipLodBias);
        }

        float MinLodClamp() {
            return ConvertFixedToFloat(minLodClamp);
        }

        float MaxLodClamp() {
            return ConvertFixedToFloat(maxLodClamp);
        }
    };
    static_assert(sizeof(TextureSamplerControl) == 0x20);

    #pragma pack(pop)
}
