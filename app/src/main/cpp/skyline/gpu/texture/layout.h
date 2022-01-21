// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "texture.h"

namespace skyline::gpu::texture {
    // Reference on Block-linear tiling: https://gist.github.com/PixelyIon/d9c35050af0ef5690566ca9f0965bc32
    constexpr u8 SectorWidth{16}; // The width of a sector in bytes
    constexpr u8 SectorHeight{2}; // The height of a sector in lines
    constexpr u8 GobWidth{64}; // The width of a GOB in bytes
    constexpr u8 GobHeight{8}; // The height of a GOB in lines

    size_t GetBlockLinearLayerSize(const GuestTexture &guest) {
        u32 blockHeight{guest.tileConfig.blockHeight}; //!< The height of the blocks in GOBs
        u32 robHeight{GobHeight * blockHeight}; //!< The height of a single ROB (Row of Blocks) in lines
        u32 surfaceHeightLines{guest.dimensions.height / guest.format->blockHeight}; //!< The height of the surface in lines
        u32 surfaceHeightRobs{util::AlignUp(surfaceHeightLines, robHeight) / robHeight}; //!< The height of the surface in ROBs (Row Of Blocks, incl. padding ROB)

        u32 robWidthBytes{util::AlignUp((guest.dimensions.width / guest.format->blockWidth) * guest.format->bpb, GobWidth)}; //!< The width of a ROB in bytes
        u32 robWidthBlocks{robWidthBytes / GobWidth}; //!< The width of a ROB in blocks (and GOBs because block width == 1 on the Tegra X1, incl. padding block)

        return robWidthBytes * robHeight * surfaceHeightRobs;
    }

    /**
     * @brief Copies pixel data between a linear and blocklinear texture
     */
     template <typename CopyFunction>
    void CopyBlockLinearInternal(const GuestTexture &guest, u8 *blockLinear, u8 *linear, CopyFunction copyFunction) {
        u32 blockHeight{guest.tileConfig.blockHeight};
        u32 robHeight{GobHeight * blockHeight};
        u32 surfaceHeightLines{guest.dimensions.height / guest.format->blockHeight};
        u32 surfaceHeightRobs{surfaceHeightLines / robHeight}; //!< The height of the surface in ROBs excluding padding ROBs

        u32 formatBpb{guest.format->bpb};
        u32 robWidthUnalignedBytes{(guest.dimensions.width / guest.format->blockWidth) * formatBpb};
        u32 robWidthBytes{util::AlignUp(robWidthUnalignedBytes, GobWidth)};
        u32 robWidthBlocks{robWidthUnalignedBytes / GobWidth};

        bool hasPaddingBlock{robWidthUnalignedBytes != robWidthBytes};
        u32 blockPaddingOffset{hasPaddingBlock ? (GobWidth - (robWidthBytes - robWidthUnalignedBytes)) : 0};

        u32 robBytes{robWidthUnalignedBytes * robHeight};
        u32 gobYOffset{robWidthUnalignedBytes * GobHeight};

        u8 *sector{blockLinear};

        auto deswizzleRob{[&](u8 *linearRob, u32 paddingY) {
            auto deswizzleBlock{[&](u8 *linearBlock, auto copySector) __attribute__((always_inline)) {
                for (u32 gobY{}; gobY < blockHeight; gobY++) { // Every Block contains `blockHeight` Y-axis GOBs
                    #pragma clang loop unroll_count(32)
                    for (u32 index{}; index < SectorWidth * SectorHeight; index++) {  // Every Y-axis GOB contains `sectorWidth * sectorHeight` sectors
                        u32 xT{((index << 3) & 0b10000) | ((index << 1) & 0b100000)}; // Morton-Swizzle on the X-axis
                        u32 yT{((index >> 1) & 0b110) | (index & 0b1)}; // Morton-Swizzle on the Y-axis

                        copySector(linearBlock + (yT * robWidthUnalignedBytes) + xT, xT);
                    }

                    linearBlock += gobYOffset; // Increment the linear GOB to the next Y-axis GOB
                }
            }};

            for (u32 block{}; block < robWidthBlocks; block++) { // Every ROB contains `surfaceWidthBlocks` blocks (excl. padding block)
                deswizzleBlock(linearRob, [&](u8 *linearSector, u32 xT) __attribute__((always_inline)) {
                    copyFunction(linearSector, sector, SectorWidth);
                    sector += SectorWidth; // `sectorWidth` bytes are of sequential image data
                });

                sector += paddingY; // Skip over any padding at the end of this block
                linearRob += GobWidth; // Increment the linear block to the next block (As Block Width = 1 GOB Width)
            }

            if (hasPaddingBlock)
                deswizzleBlock(linearRob, [&](u8 *linearSector, u32 xT) __attribute__((always_inline)) {
                    #pragma clang loop unroll_count(4)
                    for (u32 pixelOffset{}; pixelOffset < SectorWidth; pixelOffset += formatBpb) {
                        if (xT < blockPaddingOffset)
                            copyFunction(linearSector + pixelOffset, sector, formatBpb);
                        sector += formatBpb;
                        xT += formatBpb;
                    }
                });
        }};

        u8 *linearRob{linear};
        for (u32 rob{}; rob < surfaceHeightRobs; rob++) { // Every Surface contains `surfaceHeightRobs` ROBs (excl. padding ROB)
            deswizzleRob(linearRob, 0);
            linearRob += robBytes; // Increment the linear block to the next ROB
        }

        if (surfaceHeightLines % robHeight != 0) {
            blockHeight = (util::AlignUp(surfaceHeightLines, GobHeight) - (surfaceHeightRobs * robHeight)) / GobHeight; // Calculate the amount of Y GOBs which aren't padding
            deswizzleRob(linearRob, (guest.tileConfig.blockHeight - blockHeight) * (SectorWidth * SectorWidth * SectorHeight));
        }
    }

    /**
     * @brief Copies the contents of a blocklinear guest texture to a linear output buffer
     */
    void CopyBlockLinearToLinear(const GuestTexture &guest, u8 *guestInput, u8 *linearOutput) {
        CopyBlockLinearInternal(guest, guestInput, linearOutput, std::memcpy);
    }

    /**
     * @brief Copies the contents of a blocklinear guest texture to a linear output buffer
     */
    void CopyLinearToBlockLinear(const GuestTexture &guest, u8 *linearInput, u8 *guestOutput) {
        CopyBlockLinearInternal(guest, guestOutput, linearInput, [](u8* src, u8* dst, size_t size) {
            std::memcpy(dst, src, size);
        });
    }

    /**
     * @brief Copies the contents of a pitch-linear guest texture to a linear output buffer
     */
    void CopyPitchLinearToLinear(const GuestTexture &guest, u8 *guestInput, u8 *linearOutput) {
        auto sizeLine{guest.format->GetSize(guest.dimensions.width, 1)}; //!< The size of a single line of pixel data
        auto sizeStride{guest.tileConfig.pitch}; //!< The size of a single stride of pixel data

        auto inputLine{guestInput};
        auto outputLine{linearOutput};

        for (u32 line{}; line < guest.dimensions.height; line++) {
            std::memcpy(outputLine, inputLine, sizeLine);
            inputLine += sizeStride;
            outputLine += sizeLine;
        }
    }

    /**
     * @brief Copies the contents of a linear buffer to a pitch-linear guest texture
     */
    void CopyLinearToPitchLinear(const GuestTexture &guest, u8 *linearInput, u8 *guestOutput) {
        auto sizeLine{guest.format->GetSize(guest.dimensions.width, 1)}; //!< The size of a single line of pixel data
        auto sizeStride{guest.tileConfig.pitch}; //!< The size of a single stride of pixel data

        auto inputLine{linearInput};
        auto outputLine{guestOutput};

        for (u32 line{}; line < guest.dimensions.height; line++) {
            std::memcpy(outputLine, inputLine, sizeLine);
            inputLine += sizeLine;
            outputLine += sizeStride;
        }
    }
}
