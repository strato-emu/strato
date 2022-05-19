// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "layout.h"

namespace skyline::gpu::texture {
    // Reference on Block-linear tiling: https://gist.github.com/PixelyIon/d9c35050af0ef5690566ca9f0965bc32
    constexpr u8 SectorWidth{16}; // The width of a sector in bytes
    constexpr u8 SectorHeight{2}; // The height of a sector in lines
    constexpr u8 GobWidth{64}; // The width of a GOB in bytes
    constexpr u8 GobHeight{8}; // The height of a GOB in lines

    size_t GetBlockLinearLayerSize(Dimensions dimensions, size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, size_t gobBlockHeight, size_t gobBlockDepth) {
        size_t robLineWidth{util::DivideCeil<size_t>(dimensions.width, formatBlockWidth)}; //!< The width of the ROB in terms of format blocks
        size_t robLineBytes{util::AlignUp(robLineWidth * formatBpb, GobWidth)}; //!< The amount of bytes in a single block

        size_t robHeight{GobHeight * gobBlockHeight}; //!< The height of a single ROB (Row of Blocks) in lines
        size_t surfaceHeightLines{util::DivideCeil<size_t>(dimensions.height, formatBlockHeight)}; //!< The height of the surface in lines
        size_t surfaceHeightRobs{util::DivideCeil(surfaceHeightLines, robHeight)}; //!< The height of the surface in ROBs (Row Of Blocks, incl. padding ROB)

        size_t robDepth{util::AlignUp(dimensions.depth, gobBlockDepth)}; //!< The depth of the surface in slices, aligned to include padding Z-axis GOBs

        return robLineBytes * robHeight * surfaceHeightRobs * robDepth;
    }

    /**
     * @brief Copies pixel data between a linear and blocklinear texture
     */
    template<bool BlockLinearToLinear>
    void CopyBlockLinearInternal(Dimensions dimensions,
                                 size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb,
                                 size_t gobBlockHeight, size_t gobBlockDepth,
                                 u8 *blockLinear, u8 *linear) {
        size_t robWidthUnalignedBytes{(dimensions.width / formatBlockWidth) * formatBpb};
        size_t robWidthBytes{util::AlignUp(robWidthUnalignedBytes, GobWidth)};
        size_t robWidthBlocks{robWidthUnalignedBytes / GobWidth};

        size_t blockHeight{gobBlockHeight};
        size_t robHeight{GobHeight * blockHeight};
        size_t surfaceHeightLines{dimensions.height / formatBlockHeight};
        size_t surfaceHeightRobs{surfaceHeightLines / robHeight}; //!< The height of the surface in ROBs excluding padding ROBs

        size_t blockDepth{std::min<size_t>(dimensions.depth, gobBlockDepth)};
        size_t blockPaddingZ{SectorWidth * SectorHeight * blockHeight * (gobBlockDepth - blockDepth)};

        bool hasPaddingBlock{robWidthUnalignedBytes != robWidthBytes};
        size_t blockPaddingOffset{hasPaddingBlock ? (GobWidth - (robWidthBytes - robWidthUnalignedBytes)) : 0};

        size_t robBytes{robWidthUnalignedBytes * robHeight};
        size_t gobYOffset{robWidthUnalignedBytes * GobHeight};
        size_t gobZOffset{robWidthUnalignedBytes * surfaceHeightLines};

        u8 *sector{blockLinear};

        auto deswizzleRob{[&](u8 *linearRob, auto isLastRob, size_t blockPaddingY = 0, size_t blockExtentY = 0) {
            auto deswizzleBlock{[&](u8 *linearBlock, auto copySector) __attribute__((always_inline)) {
                for (size_t gobZ{}; gobZ < blockDepth; gobZ++) { // Every Block contains `blockDepth` Z-axis GOBs (Slices)
                    u8 *linearGob{linearBlock};
                    for (size_t gobY{}; gobY < blockHeight; gobY++) { // Every Block contains `blockHeight` Y-axis GOBs
                        #pragma clang loop unroll_count(32)
                        for (size_t index{}; index < SectorWidth * SectorHeight; index++) {  // Every Y-axis GOB contains `sectorWidth * sectorHeight` sectors
                            size_t xT{((index << 3) & 0b10000) | ((index << 1) & 0b100000)}; // Morton-Swizzle on the X-axis
                            size_t yT{((index >> 1) & 0b110) | (index & 0b1)}; // Morton-Swizzle on the Y-axis

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

            for (size_t block{}; block < robWidthBlocks; block++) { // Every ROB contains `surfaceWidthBlocks` blocks (excl. padding block)
                deswizzleBlock(linearRob, [&](u8 *linearSector, size_t) __attribute__((always_inline)) {
                    if constexpr (BlockLinearToLinear)
                        std::memcpy(linearSector, sector, SectorWidth);
                    else
                        std::memcpy(sector, linearSector, SectorWidth);
                    sector += SectorWidth; // `sectorWidth` bytes are of sequential image data
                });

                if constexpr (isLastRob)
                    sector += blockPaddingY; // Skip over any padding at the end of this block
                linearRob += GobWidth; // Increment the linear block to the next block (As Block Width = 1 GOB Width)
            }

            if (hasPaddingBlock)
                deswizzleBlock(linearRob, [&](u8 *linearSector, size_t xT) __attribute__((always_inline)) {
                    #pragma clang loop unroll_count(4)
                    for (size_t pixelOffset{}; pixelOffset < SectorWidth; pixelOffset += formatBpb) {
                        if (xT < blockPaddingOffset)
                            if constexpr (BlockLinearToLinear)
                                std::memcpy(linearSector + pixelOffset, sector, formatBpb);
                            else
                                std::memcpy(sector, linearSector + pixelOffset, formatBpb);

                        sector += formatBpb;
                        xT += formatBpb;
                    }
                });
        }};

        u8 *linearRob{linear};
        for (size_t rob{}; rob < surfaceHeightRobs; rob++) { // Every Surface contains `surfaceHeightRobs` ROBs (excl. padding ROB)
            deswizzleRob(linearRob, std::false_type{});
            linearRob += robBytes; // Increment the linear ROB to the next ROB
        }

        if (surfaceHeightLines % robHeight != 0) {
            blockHeight = (util::AlignUp(surfaceHeightLines, GobHeight) - (surfaceHeightRobs * robHeight)) / GobHeight; // Calculate the amount of Y GOBs which aren't padding

            size_t alignedSurfaceLines{util::DivideCeil<size_t>(dimensions.height, formatBlockHeight)};
            deswizzleRob(
                linearRob,
                std::true_type{},
                (gobBlockHeight - blockHeight) * (SectorWidth * SectorWidth * SectorHeight), // Calculate padding at the end of a block to skip
                util::IsAligned(alignedSurfaceLines, GobHeight) ? GobHeight : alignedSurfaceLines - util::AlignDown(alignedSurfaceLines, GobHeight) // Calculate the line relative to the start of the last GOB that is the cut-off point for the image
            );
        }
    }

    void CopyBlockLinearToLinear(Dimensions dimensions, size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, size_t gobBlockHeight, size_t gobBlockDepth, u8 *blockLinear, u8 *linear) {
        CopyBlockLinearInternal<true>(
            dimensions,
            formatBlockWidth, formatBlockHeight, formatBpb,
            gobBlockHeight, gobBlockDepth,
            blockLinear, linear
        );
    }

    void CopyBlockLinearToLinear(const GuestTexture &guest, u8 *blockLinear, u8 *linear) {
        CopyBlockLinearInternal<true>(
            guest.dimensions,
            guest.format->blockWidth, guest.format->blockHeight, guest.format->bpb,
            guest.tileConfig.blockHeight, guest.tileConfig.blockDepth,
            blockLinear, linear
        );
    }

    void CopyLinearToBlockLinear(Dimensions dimensions, size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, size_t gobBlockHeight, size_t gobBlockDepth, u8 *linear, u8 *blockLinear) {
        CopyBlockLinearInternal<false>(
            dimensions,
            formatBlockWidth, formatBlockHeight, formatBpb,
            gobBlockHeight, gobBlockDepth,
            blockLinear, linear
        );
    }

    void CopyLinearToBlockLinear(const GuestTexture &guest, u8 *linear, u8 *blockLinear) {
        CopyBlockLinearInternal<false>(
            guest.dimensions,
            guest.format->blockWidth, guest.format->blockHeight, guest.format->bpb,
            guest.tileConfig.blockHeight, guest.tileConfig.blockDepth,
            blockLinear, linear
        );
    }

    void CopyPitchLinearToLinear(const GuestTexture &guest, u8 *guestInput, u8 *linearOutput) {
        auto sizeLine{guest.format->GetSize(guest.dimensions.width, 1)}; //!< The size of a single line of pixel data
        auto sizeStride{guest.tileConfig.pitch}; //!< The size of a single stride of pixel data

        auto inputLine{guestInput};
        auto outputLine{linearOutput};

        for (size_t line{}; line < guest.dimensions.height; line++) {
            std::memcpy(outputLine, inputLine, sizeLine);
            inputLine += sizeStride;
            outputLine += sizeLine;
        }
    }

    void CopyLinearToPitchLinear(const GuestTexture &guest, u8 *linearInput, u8 *guestOutput) {
        auto sizeLine{guest.format->GetSize(guest.dimensions.width, 1)}; //!< The size of a single line of pixel data
        auto sizeStride{guest.tileConfig.pitch}; //!< The size of a single stride of pixel data

        auto inputLine{linearInput};
        auto outputLine{guestOutput};

        for (size_t line{}; line < guest.dimensions.height; line++) {
            std::memcpy(outputLine, inputLine, sizeLine);
            inputLine += sizeLine;
            outputLine += sizeStride;
        }
    }
}
