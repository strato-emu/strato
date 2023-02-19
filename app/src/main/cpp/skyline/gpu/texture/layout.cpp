// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "layout.h"

namespace skyline::gpu::texture {
    // Reference on Block-linear tiling: https://gist.github.com/PixelyIon/d9c35050af0ef5690566ca9f0965bc32
    constexpr size_t SectorWidth{16}; //!< The width of a sector in bytes
    constexpr size_t SectorHeight{2}; //!< The height of a sector in lines
    constexpr size_t GobWidth{64}; //!< The width of a GOB in bytes
    constexpr size_t GobHeight{8}; //!< The height of a GOB in lines
    constexpr size_t SectorLinesInGob{(GobWidth / SectorWidth) * GobHeight}; //!< The number of lines of sectors inside a GOB

    size_t GetBlockLinearLayerSize(Dimensions dimensions, size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, size_t gobBlockHeight, size_t gobBlockDepth) {
        size_t robLineWidth{util::DivideCeil<size_t>(dimensions.width, formatBlockWidth)}; //!< The width of the ROB in terms of format blocks
        size_t robLineBytes{util::AlignUp(robLineWidth * formatBpb, GobWidth)}; //!< The amount of bytes in a single block

        size_t robHeight{GobHeight * gobBlockHeight}; //!< The height of a single ROB (Row of Blocks) in lines
        size_t surfaceHeightLines{util::DivideCeil<size_t>(dimensions.height, formatBlockHeight)}; //!< The height of the surface in lines
        size_t surfaceHeightRobs{util::DivideCeil(surfaceHeightLines, robHeight)}; //!< The height of the surface in ROBs (Row Of Blocks, incl. padding ROB)

        size_t robDepth{util::AlignUp(dimensions.depth, gobBlockDepth)}; //!< The depth of the surface in slices, aligned to include padding Z-axis GOBs

        return robLineBytes * robHeight * surfaceHeightRobs * robDepth;
    }

    template<typename Type>
    constexpr Type CalculateBlockGobs(Type blockGobs, Type surfaceGobs) {
        if (surfaceGobs > blockGobs)
            return blockGobs;
        return std::bit_ceil<Type>(surfaceGobs);
    }

    size_t GetBlockLinearLayerSize(Dimensions dimensions, size_t formatBlockHeight, size_t formatBlockWidth, size_t formatBpb, size_t gobBlockHeight, size_t gobBlockDepth, size_t levelCount, bool isMultiLayer) {
        // Calculate the size of the surface in GOBs on every axis
        size_t gobsWidth{util::DivideCeil<size_t>(util::DivideCeil<size_t>(dimensions.width, formatBlockWidth) * formatBpb, GobWidth)};
        size_t gobsHeight{util::DivideCeil<size_t>(util::DivideCeil<size_t>(dimensions.height, formatBlockHeight), GobHeight)};
        size_t gobsDepth{dimensions.depth};

        size_t totalSize{}, layerAlignment{GobWidth * GobHeight * gobBlockHeight * gobBlockDepth};
        for (size_t i{}; i < levelCount; i++) {
            // Iterate over every level, adding the size of the current level to the total size
            totalSize += (GobWidth * gobsWidth) * (GobHeight * util::AlignUp(gobsHeight, gobBlockHeight)) * util::AlignUp(gobsDepth, gobBlockDepth);

            // Successively divide every dimension by 2 until the final level is reached, the division is rounded up to contain the padding GOBs
            gobsWidth = std::max(util::DivideCeil(gobsWidth, 2UL), 1UL);
            gobsHeight = std::max(util::DivideCeil(gobsHeight, 2UL), 1UL);
            gobsDepth = std::max(gobsDepth / 2, 1UL); // The GOB depth is the same as the depth dimension and needs to be rounded down during the division

            gobBlockHeight = CalculateBlockGobs(gobBlockHeight, gobsHeight);
            gobBlockDepth = CalculateBlockGobs(gobBlockDepth, gobsDepth);
        }

        return isMultiLayer ? util::AlignUp(totalSize, layerAlignment) : totalSize;
    }

    std::vector<MipLevelLayout> GetBlockLinearMipLayout(Dimensions dimensions, size_t formatBlockHeight, size_t formatBlockWidth, size_t formatBpb, size_t targetFormatBlockHeight, size_t targetFormatBlockWidth, size_t targetFormatBpb, size_t gobBlockHeight, size_t gobBlockDepth, size_t levelCount) {
        std::vector<MipLevelLayout> mipLevels;
        mipLevels.reserve(levelCount);

        size_t gobsWidth{util::DivideCeil<size_t>(util::DivideCeil<size_t>(dimensions.width, formatBlockWidth) * formatBpb, GobWidth)};
        size_t gobsHeight{util::DivideCeil<size_t>(util::DivideCeil<size_t>(dimensions.height, formatBlockHeight), GobHeight)};
        // Note: We don't need a separate gobsDepth variable here, since a GOB is always a single slice deep and the value would be the same as the depth dimension

        for (size_t i{}; i < levelCount; i++) {
            size_t linearSize{util::DivideCeil<size_t>(dimensions.width, formatBlockWidth) * formatBpb * util::DivideCeil<size_t>(dimensions.height, formatBlockHeight) * dimensions.depth};
            size_t targetLinearSize{targetFormatBpb == 0 ? linearSize : util::DivideCeil<size_t>(dimensions.width, targetFormatBlockWidth) * targetFormatBpb * util::DivideCeil<size_t>(dimensions.height, targetFormatBlockHeight) * dimensions.depth};

            mipLevels.emplace_back(
                dimensions,
                linearSize,
                targetLinearSize,
                (GobWidth * gobsWidth) * (GobHeight * util::AlignUp(gobsHeight, gobBlockHeight)) * util::AlignUp(dimensions.depth, gobBlockDepth),
                gobBlockHeight, gobBlockDepth
            );

            gobsWidth = std::max(util::DivideCeil(gobsWidth, 2UL), 1UL);
            gobsHeight = std::max(util::DivideCeil(gobsHeight, 2UL), 1UL);

            dimensions.width = std::max(dimensions.width / 2, 1U);
            dimensions.height = std::max(dimensions.height / 2, 1U);
            dimensions.depth = std::max(dimensions.depth / 2, 1U);

            gobBlockHeight = CalculateBlockGobs(gobBlockHeight, gobsHeight);
            gobBlockDepth = CalculateBlockGobs(gobBlockDepth, static_cast<size_t>(dimensions.depth));
        }

        return mipLevels;
    }

    /**
     * @brief Copies pixel data between a pitch-linear and blocklinear texture
     * @tparam BlockLinearToPitch Whether to copy from a blocklinear texture to a pitch-linear texture or a pitch-linear texture to a blocklinear texture
     */
    template<bool BlockLinearToPitch>
    void CopyBlockLinearInternal(Dimensions dimensions,
                                 size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, u32 pitchAmount,
                                 size_t gobBlockHeight, size_t gobBlockDepth,
                                 u8 *blockLinear, u8 *pitch) {
        size_t robWidthUnalignedBytes{util::DivideCeil<size_t>(dimensions.width, formatBlockWidth) * formatBpb};
        size_t robWidthBytes{util::AlignUp(robWidthUnalignedBytes, GobWidth)};
        size_t robWidthBlocks{robWidthUnalignedBytes / GobWidth};

        if (formatBpb == 12) [[unlikely]]
            formatBpb = 4;

        size_t blockHeight{gobBlockHeight};
        size_t robHeight{GobHeight * blockHeight};
        size_t surfaceHeightLines{util::DivideCeil<size_t>(dimensions.height, formatBlockHeight)};
        size_t surfaceHeightRobs{surfaceHeightLines / robHeight}; //!< The height of the surface in ROBs excluding padding ROBs

        size_t blockDepth{std::min<size_t>(dimensions.depth, gobBlockDepth)};
        size_t blockPaddingZ{GobWidth * GobHeight * blockHeight * (gobBlockDepth - blockDepth)};

        bool hasPaddingBlock{robWidthUnalignedBytes != robWidthBytes};
        size_t blockPaddingOffset{hasPaddingBlock ? (GobWidth - (robWidthBytes - robWidthUnalignedBytes)) : 0};

        size_t pitchWidthBytes{pitchAmount ? pitchAmount : robWidthUnalignedBytes};
        size_t robBytes{pitchWidthBytes * robHeight};
        size_t gobYOffset{pitchWidthBytes * GobHeight};
        size_t gobZOffset{pitchWidthBytes * surfaceHeightLines};

        u8 *sector{blockLinear};

        auto deswizzleRob{[&](u8 *pitchRob, auto isLastRob, size_t blockPaddingY = 0, size_t blockExtentY = 0) {
            auto deswizzleBlock{[&](u8 *pitchBlock, auto copySector) __attribute__((always_inline)) {
                for (size_t gobZ{}; gobZ < blockDepth; gobZ++) { // Every Block contains `blockDepth` Z-axis GOBs (Slices)
                    u8 *pitchGob{pitchBlock};
                    for (size_t gobY{}; gobY < blockHeight; gobY++) { // Every Block contains `blockHeight` Y-axis GOBs
                        #pragma clang loop unroll_count(SectorLinesInGob)
                        for (size_t index{}; index < SectorLinesInGob; index++) {
                            size_t xT{((index << 3) & 0b10000) | ((index << 1) & 0b100000)}; // Morton-Swizzle on the X-axis
                            size_t yT{((index >> 1) & 0b110) | (index & 0b1)}; // Morton-Swizzle on the Y-axis

                            if constexpr (!isLastRob) {
                                copySector(pitchGob + (yT * pitchWidthBytes) + xT, xT);
                            } else {
                                if (gobY != blockHeight - 1 || yT < blockExtentY)
                                    copySector(pitchGob + (yT * pitchWidthBytes) + xT, xT);
                                else
                                    sector += SectorWidth;
                            }
                        }

                        pitchGob += gobYOffset; // Increment the linear GOB to the next Y-axis GOB
                    }

                    if constexpr (isLastRob)
                        sector += blockPaddingY; // Skip over any padding at the end of this slice

                    pitchBlock += gobZOffset; // Increment the linear block to the next Z-axis GOB
                }

                sector += blockPaddingZ; // Skip over any padding Z-axis GOBs
            }};

            for (size_t block{}; block < robWidthBlocks; block++) { // Every ROB contains `surfaceWidthBlocks` blocks (excl. padding block)
                deswizzleBlock(pitchRob, [&](u8 *linearSector, size_t) __attribute__((always_inline)) {
                    if constexpr (BlockLinearToPitch)
                        std::memcpy(linearSector, sector, SectorWidth);
                    else
                        std::memcpy(sector, linearSector, SectorWidth);
                    sector += SectorWidth; // `sectorWidth` bytes are of sequential image data
                });

                pitchRob += GobWidth; // Increment the linear block to the next block (As Block Width = 1 GOB Width)
            }

            if (hasPaddingBlock)
                deswizzleBlock(pitchRob, [&](u8 *linearSector, size_t xT) __attribute__((always_inline)) {
                    #pragma clang loop unroll_count(4)
                    for (size_t pixelOffset{}; pixelOffset < SectorWidth; pixelOffset += formatBpb) {
                        if (xT < blockPaddingOffset) {
                            if constexpr (BlockLinearToPitch)
                                std::memcpy(linearSector + pixelOffset, sector, formatBpb);
                            else
                                std::memcpy(sector, linearSector + pixelOffset, formatBpb);
                        }

                        sector += formatBpb;
                        xT += formatBpb;
                    }
                });
        }};

        u8 *pitchRob{pitch};
        for (size_t rob{}; rob < surfaceHeightRobs; rob++) { // Every Surface contains `surfaceHeightRobs` ROBs (excl. padding ROB)
            deswizzleRob(pitchRob, std::false_type{});
            pitchRob += robBytes; // Increment the linear ROB to the next ROB
        }

        if (surfaceHeightLines % robHeight != 0) {
            blockHeight = (util::AlignUp(surfaceHeightLines, GobHeight) - (surfaceHeightRobs * robHeight)) / GobHeight; // Calculate the amount of Y GOBs which aren't padding

            deswizzleRob(
                pitchRob,
                std::true_type{},
                (gobBlockHeight - blockHeight) * (GobWidth * GobHeight), // Calculate padding at the end of a block to skip
                util::IsAligned(surfaceHeightLines, GobHeight) ? GobHeight : surfaceHeightLines - util::AlignDown(surfaceHeightLines, GobHeight) // Calculate the line relative to the start of the last GOB that is the cut-off point for the image
            );
        }
    }

    void CopyBlockLinearToLinear(Dimensions dimensions, size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, size_t gobBlockHeight, size_t gobBlockDepth, u8 *blockLinear, u8 *linear) {
        CopyBlockLinearInternal<true>(
            dimensions,
            formatBlockWidth, formatBlockHeight, formatBpb, 0,
            gobBlockHeight, gobBlockDepth,
            blockLinear, linear
        );
    }

    void CopyBlockLinearToPitch(Dimensions dimensions,
                                size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, u32 pitchAmount,
                                size_t gobBlockHeight, size_t gobBlockDepth,
                                u8 *blockLinear, u8 *pitch) {
        CopyBlockLinearInternal<true>(
            dimensions,
            formatBlockWidth, formatBlockHeight, formatBpb, pitchAmount,
            gobBlockHeight, gobBlockDepth,
            blockLinear, pitch
        );
    }
    void CopyBlockLinearToLinear(const GuestTexture &guest, u8 *blockLinear, u8 *linear) {
        CopyBlockLinearInternal<true>(
            guest.dimensions,
            guest.format->blockWidth, guest.format->blockHeight, guest.format->bpb, 0,
            guest.tileConfig.blockHeight, guest.tileConfig.blockDepth,
            blockLinear, linear
        );
    }

    void CopyLinearToBlockLinear(Dimensions dimensions,
                                 size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb,
                                 size_t gobBlockHeight, size_t gobBlockDepth,
                                 u8 *linear, u8 *blockLinear) {
        CopyBlockLinearInternal<false>(dimensions,
                                       formatBlockWidth, formatBlockHeight, formatBpb, 0,
                                       gobBlockHeight, gobBlockDepth,
                                       blockLinear, linear
        );
    }

    void CopyPitchToBlockLinear(Dimensions dimensions, size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, u32 pitchAmount, size_t gobBlockHeight, size_t gobBlockDepth, u8 *pitch, u8 *blockLinear) {
        CopyBlockLinearInternal<false>(
            dimensions,
            formatBlockWidth, formatBlockHeight, formatBpb, pitchAmount,
            gobBlockHeight, gobBlockDepth,
            blockLinear, pitch
        );
    }
        );
    }

    void CopyLinearToBlockLinear(const GuestTexture &guest, u8 *linear, u8 *blockLinear) {
        CopyBlockLinearInternal<false>(
            guest.dimensions,
            guest.format->blockWidth, guest.format->blockHeight, guest.format->bpb, 0,
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
