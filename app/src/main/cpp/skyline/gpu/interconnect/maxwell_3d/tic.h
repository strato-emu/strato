// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#pragma once

#include <bit>
#include <common/base.h>

namespace skyline::gpu::interconnect {
    #pragma pack(push, 1)

    /**
     * @brief The Texture Image Control is a descriptor used to configure the texture unit in Maxwell GPUs
     * @url https://github.com/envytools/envytools/blob/master/rnndb/graph/gm200_texture.xml
     * @url https://github.com/devkitPro/deko3d/blob/00c12d1f4809014f1cc22719dd2e3476735eec64/source/maxwell/texture_image_control_block.h
     * @note Any members with underscore number suffixes represent a bitfield range of a value that member represents
     * @note Any enumerations that have numerical enumerants are prefixed with 'e'
     */
    struct TextureImageControl {
        /**
         * @note An underscore may be used to describe a different block in a format
         */
        enum class ImageFormat : u32 {
            Invalid = 0x0,
            R32G32B32A32 = 0x01,
            R32G32B32 = 0x02,
            R16G16B16A16 = 0x03,
            R32G32 = 0x04,
            R32B24G8 = 0x05,
            Etc2Rgb = 0x06,
            X8B8G8R8 = 0x07,
            A8B8G8R8 = 0x08,
            A2B10G10R10 = 0x09,
            Etc2RgbPta = 0x0A,
            Etc2Rgba = 0x0B,
            R16G16 = 0x0C,
            R24G8 = 0x0D,
            R8G24 = 0x0E,
            R32 = 0x0F,
            Bc6HSfloat = 0x10,
            Bc6HUfloat = 0x11,
            R4G4B4A4 = 0x12,
            A5B5G5R1 = 0x13,
            A1B5G5R5 = 0x14,
            B5G6R5 = 0x15,
            B6G5R5 = 0x16,
            BC7 = 0x17,
            R8G8 = 0x18,
            Eac = 0x19,
            EacX2 = 0x1A,
            R16 = 0x1B,
            Y8Video = 0x1C,
            R8 = 0x1D,
            G4R4 = 0x1E,
            R1 = 0x1F,
            E5B9G9R9 = 0x20,
            B10G11R11 = 0x21,
            G8B8G8R8 = 0x22,
            B8G8R8G8 = 0x23,
            BC1 = 0x24,
            BC2 = 0x25,
            BC3 = 0x26,
            BC4 = 0x27,
            BC5 = 0x28,
            S8D24 = 0x29,
            X8D24 = 0x2A,
            D24S8 = 0x2B,
            X4V4D24_Cov4R4V = 0x2C,
            X4V4D24_Cov8R8V = 0x2D,
            V8D24_Cov4R12V = 0x2E,
            D32 = 0x2F,
            D32S8 = 0x30,
            X8D24_X20V4S8_Cov4R4V = 0x31,
            X8D24_X20V4S8_Cov8R8V = 0x32,
            D32_X20V4X8_Cov4R4V = 0x33,
            D32_X20V4X8_Cov8R8V = 0x34,
            D32_X20V4S8_Cov4R4V = 0x35,
            D32_X20V4S8_Cov8R8V = 0x36,
            X8D24_X16V8S8_Cov4R12V = 0x37,
            D32_X16V8X8_Cov4R12V = 0x38,
            D32_X16V8S8_Cov4R12V = 0x39,
            D16 = 0x3A,
            V8D24_Cov8R24V = 0x3B,
            X8D24_X16V8S8_Cov8R24V = 0x3C,
            D32_X16V8X8_Cov8R24V = 0x3D,
            D32_X16V8S8_Cov8R24V = 0x3E,
            Astc4x4 = 0x40,
            Astc5x5 = 0x41,
            Astc6x6 = 0x42,
            Astc8x8 = 0x44,
            Astc10x10 = 0x45,
            Astc12x12 = 0x46,
            Astc5x4 = 0x50,
            Astc6x5 = 0x51,
            Astc8x6 = 0x52,
            Astc10x8 = 0x53,
            Astc12x10 = 0x54,
            Astc8x5 = 0x55,
            Astc10x5 = 0x56,
            Astc10x6 = 0x57,
        };

        enum class ImageComponent : u32 {
            Snorm = 1,
            Unorm = 2,
            Sint = 3,
            Uint = 4,
            SnormForceFp16 = 5,
            UnormForceFp16 = 6,
            Float = 7,
        };

        enum class ImageSwizzle : u32 {
            Zero = 0,
            R = 2,
            G = 3,
            B = 4,
            A = 5,
            OneInt = 6,
            OneFloat = 7,
        };

        enum class HeaderType : u32 {
            Buffer1D = 0,
            PitchColorKey = 1,
            Pitch = 2,
            BlockLinear = 3,
            BlockLinearColorKey = 4,
        };

        enum class TextureType : u32 {
            e1D = 0,
            e2D = 1,
            e3D = 2,
            eCube = 3,
            e1DArray = 4,
            e2DArray = 5,
            e1DBuffer = 6,
            e2DNoMipmap = 7,
            eCubeArray = 8,
        };

        enum class MsaaMode : u32 {
            e1x1 = 0,
            e2x1 = 1,
            e2x2 = 2,
            e4x2 = 3,
            e4x2D3D = 4,
            e2x1D3D = 5,
            e4x4 = 6,
            e2x2Vc4 = 8,
            e2x2Vc12 = 9,
            e4x2Vc8 = 10,
            e4x2Vc24 = 11,
        };

        enum class LodQuality : u32 {
            Low = 0,
            High = 1,
        };

        enum class SectorPromotion : u32 {
            None = 0,
            To2V = 1,
            To2H = 2,
            To4 = 3,
        };

        enum class BorderSize : u32 {
            One = 0,
            Two = 1,
            Four = 2,
            Eight = 3,
            SamplerColor = 7,
        };

        enum class AnisotropySpreadModifier : u32 {
            None = 0,
            One = 1,
            Two = 2,
            Sqrt = 3,
        };

        enum class AnisotropySpread : u32 {
            Half = 0,
            One = 1,
            Two = 2,
            Max = 3,
        };

        enum class MaxAnisotropy : u32 {
            e1to1 = 0,
            e2to1 = 1,
            e4to1 = 2,
            e6to1 = 3,
            e8to1 = 4,
            e10to1 = 5,
            e12to1 = 6,
            e16to1 = 7,
        };

        // 0x00
        struct FormatWord {
            static constexpr u32 FormatColorComponentPadMask{(1U << 31) | 0b111'111'111'111'1111111U}; //!< Mask for the format, component and pad fields

            ImageFormat format : 7;
            ImageComponent componentR : 3;
            ImageComponent componentG : 3;
            ImageComponent componentB : 3;
            ImageComponent componentA : 3;
            ImageSwizzle swizzleX : 3;
            ImageSwizzle swizzleY : 3;
            ImageSwizzle swizzleZ : 3;
            ImageSwizzle swizzleW : 3;
            u32 _pad_ : 1;

            bool operator==(const FormatWord &) const = default;

            constexpr u32 Raw() const {
                if (std::is_constant_evaluated()) {
                    u32 raw{_pad_};
                    raw <<= 3;
                    raw |= static_cast<u32>(swizzleW);
                    raw <<= 3;
                    raw |= static_cast<u32>(swizzleZ);
                    raw <<= 3;
                    raw |= static_cast<u32>(swizzleY);
                    raw <<= 3;
                    raw |= static_cast<u32>(swizzleX);
                    raw <<= 3;
                    raw |= static_cast<u32>(componentA);
                    raw <<= 3;
                    raw |= static_cast<u32>(componentB);
                    raw <<= 3;
                    raw |= static_cast<u32>(componentG);
                    raw <<= 3;
                    raw |= static_cast<u32>(componentR);
                    raw <<= 7;
                    raw |= static_cast<u32>(format);
                    return raw;
                } else {
                    return util::BitCast<u32>(*this);
                }
            }
        } formatWord;

        // 0x04
        u32 addressLow;

        // 0x08
        u32 addressHigh : 16;
        u32 viewLayerBase_3_7 : 5;
        HeaderType headerType : 3;
        u32 loadStoreHint : 1;
        u32 viewCoherencyHash : 4;
        u32 viewLayerBase_8_10 : 3;

        // 0x0C
        union TileConfig {
            u16 widthMinusOne_16_31;

            constexpr static size_t PitchAlignmentBits{5}; //!< The amount of bits that are 0 in the pitch due to alignment (Aligned to 32 bytes)
            u16 pitchHigh; //!< Upper 16-bits of the 21-bit pitch, lower bits are implicitly zero due to alignment
            struct {
                u16 tileWidthGobsLog2 : 3;
                u16 tileHeightGobsLog2 : 3;
                u16 tileDepthGobsLog2 : 3;
                u16 _pad0_ : 1;
                u16 sparseTileWidthGobsLog2 : 3;
                u16 gob3d : 1;
                u16 _pad1_ : 2;
            };
            u16 raw;

            bool operator==(const TileConfig &other) const {
                return raw == other.raw;
            }
        } tileConfig;
        u16 lodAnisotropyQuality_2 : 1;
        LodQuality lodAnisotropyQuality : 1;
        LodQuality lodIsotropyQuality : 1;
        AnisotropySpreadModifier anisotropyCoarseSpreadModifier : 2;
        u16 anisotropySpreadScale : 5;
        u16 useHeaderOptControl : 1;
        u16 depthTexture : 1;
        u16 mipMaxLevels : 4;

        // 0x10
        u32 widthMinusOne : 16;
        u32 viewLayerBase_0_2 : 3;
        u32 anisotropySpreadMaxLog2 : 3;
        u32 isSrgb : 1;
        TextureType textureType : 4;
        SectorPromotion sectorPromotion : 2;
        BorderSize borderSize : 3;

        // 0x14
        u32 heightMinusOne : 16;
        u32 depthMinusOne : 14;
        u32 isSparse : 1;
        u32 normalizedCoordinates : 1;

        // 0x18
        u32 colorKeyOp : 1;
        u32 trilinOpt : 5;
        u32 mipLodBias : 13;
        u32 anisoBias : 4;
        AnisotropySpread anisotropyFineSpread : 2;
        AnisotropySpread anisotropyCoarseSpread : 2;
        MaxAnisotropy maxAnisotropy : 3;
        AnisotropySpreadModifier anisotropyFineSpreadModifier : 2;

        // 0x1C
        union ViewConfig {
            u32 colorKeyValue;
            struct {
                u32 mipMinLevel : 4;
                u32 mipMaxLevel : 4;
                MsaaMode msaaMode : 4;
                u32 minLodClamp : 12;
                u32 _pad2_ : 8;
            };
            u32 raw;

            bool operator==(const ViewConfig &other) const {
                return raw == other.raw;
            }
        } viewConfig;

        bool operator==(const TextureImageControl &) const = default;

        u64 Iova() const {
            return (static_cast<u64>(addressHigh) << 32) | addressLow;
        }

        u32 BaseLayer() const {
            return static_cast<u32>(viewLayerBase_0_2 | (viewLayerBase_3_7 << 3) | (viewLayerBase_8_10 << 8));
        }
    };
    static_assert(sizeof(TextureImageControl) == 0x20);

    #pragma pack(pop)
}
