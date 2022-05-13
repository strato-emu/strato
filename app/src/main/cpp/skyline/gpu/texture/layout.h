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

    inline size_t GetBlockLinearLayerSize(const GuestTexture &guest) {
        u32 blockBytes{util::AlignUp((guest.dimensions.width / guest.format->blockWidth) * guest.format->bpb, GobWidth)}; //!< The amount of bytes in a single block

        u32 robHeight{GobHeight * static_cast<u32>(guest.tileConfig.blockHeight)}; //!< The height of a single ROB (Row of Blocks) in lines
        u32 surfaceHeightLines{util::DivideCeil(guest.dimensions.height, u32{guest.format->blockHeight})}; //!< The height of the surface in lines
        u32 surfaceHeightRobs{util::DivideCeil(surfaceHeightLines, robHeight)}; //!< The height of the surface in ROBs (Row Of Blocks, incl. padding ROB)

        u32 robDepth{util::AlignUp(guest.dimensions.depth, guest.tileConfig.blockDepth)}; //!< The depth of the surface in slices, aligned to include padding Z-axis GOBs

        return blockBytes * robHeight * surfaceHeightRobs * robDepth;
    }

    /**
     * @brief Copies pixel data between a linear and blocklinear texture
     */
    template<typename CopyFunction>
    void CopyBlockLinearInternal(const GuestTexture &guest, u8 *blockLinear, u8 *linear, CopyFunction copyFunction) {
        u32 formatBpb{guest.format->bpb};
        u32 robWidthUnalignedBytes{(guest.dimensions.width / guest.format->blockWidth) * formatBpb};
        u32 robWidthBytes{util::AlignUp(robWidthUnalignedBytes, GobWidth)};
        u32 robWidthBlocks{robWidthUnalignedBytes / GobWidth};

        u32 blockHeight{guest.tileConfig.blockHeight};
        u32 robHeight{GobHeight * blockHeight};
        u32 surfaceHeightLines{guest.dimensions.height / guest.format->blockHeight};
        u32 surfaceHeightRobs{surfaceHeightLines / robHeight}; //!< The height of the surface in ROBs excluding padding ROBs

        u32 blockDepth{std::min(guest.dimensions.depth, static_cast<u32>(guest.tileConfig.blockDepth))};
        u32 blockPaddingZ{SectorWidth * SectorHeight * blockHeight * (guest.tileConfig.blockDepth - blockDepth)};

        bool hasPaddingBlock{robWidthUnalignedBytes != robWidthBytes};
        u32 blockPaddingOffset{hasPaddingBlock ? (GobWidth - (robWidthBytes - robWidthUnalignedBytes)) : 0};

        u32 robBytes{robWidthUnalignedBytes * robHeight};
        u32 gobYOffset{robWidthUnalignedBytes * GobHeight};
        u32 gobZOffset{robWidthUnalignedBytes * surfaceHeightLines};

        u8 *sector{blockLinear};

        auto deswizzleRob{[&](u8 *linearRob, auto isLastRob, u32 blockPaddingY = 0, u32 blockExtentY = 0) {
            auto deswizzleBlock{[&](u8 *linearBlock, auto copySector) __attribute__((always_inline)) {
                for (u32 gobZ{}; gobZ < blockDepth; gobZ++) { // Every Block contains `blockDepth` Z-axis GOBs (Slices)
                    u8 *linearGob{linearBlock};
                    for (u32 gobY{}; gobY < blockHeight; gobY++) { // Every Block contains `blockHeight` Y-axis GOBs
                        #pragma clang loop unroll_count(32)
                        for (u32 index{}; index < SectorWidth * SectorHeight; index++) {  // Every Y-axis GOB contains `sectorWidth * sectorHeight` sectors
                            u32 xT{((index << 3) & 0b10000) | ((index << 1) & 0b100000)}; // Morton-Swizzle on the X-axis
                            u32 yT{((index >> 1) & 0b110) | (index & 0b1)}; // Morton-Swizzle on the Y-axis

                            if constexpr (!isLastRob) {
                                copySector(linearGob + (yT * robWidthUnalignedBytes) + xT, xT);
                            } else {
                                if (gobY != blockHeight - 1 || yT < blockExtentY)
                                    copySector(linearGob + (yT * robWidthUnalignedBytes) + xT, xT);
                                else
                                    sector += SectorWidth;
                            }
                        }

                        linearGob += gobYOffset; // Increment the linear GOB to the next Y-axis GOB
                    }

                    linearBlock += gobZOffset; // Increment the linear block to the next Z-axis GOB
                }

                sector += blockPaddingZ; // Skip over any padding Z-axis GOBs
            }};

            for (u32 block{}; block < robWidthBlocks; block++) { // Every ROB contains `surfaceWidthBlocks` blocks (excl. padding block)
                deswizzleBlock(linearRob, [&](u8 *linearSector, u32) __attribute__((always_inline)) {
                    copyFunction(linearSector, sector, SectorWidth);
                    sector += SectorWidth; // `sectorWidth` bytes are of sequential image data
                });

                if constexpr (isLastRob)
                    sector += blockPaddingY; // Skip over any padding at the end of this block
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
            deswizzleRob(linearRob, std::false_type{});
            linearRob += robBytes; // Increment the linear ROB to the next ROB
        }

        if (surfaceHeightLines % robHeight != 0) {
            blockHeight = (util::AlignUp(surfaceHeightLines, GobHeight) - (surfaceHeightRobs * robHeight)) / GobHeight; // Calculate the amount of Y GOBs which aren't padding

            u32 alignedSurfaceLines{util::DivideCeil(guest.dimensions.height, u32{guest.format->blockHeight})};
            deswizzleRob(
                linearRob,
                std::true_type{},
                (guest.tileConfig.blockHeight - blockHeight) * (SectorWidth * SectorWidth * SectorHeight), // Calculate padding at the end of a block to skip
                util::IsAligned(alignedSurfaceLines, GobHeight) ? GobHeight : alignedSurfaceLines - util::AlignDown(alignedSurfaceLines, GobHeight) // Calculate the line relative to the start of the last GOB that is the cut-off point for the image
            );
        }
    }

    /**
     * @brief Copies the contents of a blocklinear guest texture to a linear output buffer
     */
    inline void CopyBlockLinearToLinear(const GuestTexture &guest, u8 *guestInput, u8 *linearOutput) {
        CopyBlockLinearInternal(guest, guestInput, linearOutput, std::memcpy);
    }

    /**
     * @brief Copies the contents of a blocklinear guest texture to a linear output buffer
     */
    inline void CopyLinearToBlockLinear(const GuestTexture &guest, u8 *linearInput, u8 *guestOutput) {
        CopyBlockLinearInternal(guest, guestOutput, linearInput, [](u8 *src, u8 *dst, size_t size) {
            std::memcpy(dst, src, size);
        });
    }

    /**
     * @brief Copies the contents of a pitch-linear guest texture to a linear output buffer
     */
    inline void CopyPitchLinearToLinear(const GuestTexture &guest, u8 *guestInput, u8 *linearOutput) {
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
    inline void CopyLinearToPitchLinear(const GuestTexture &guest, u8 *linearInput, u8 *guestOutput) {
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
