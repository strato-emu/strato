// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "layout.h"

namespace skyline::gpu::texture {
    #pragma pack(push, 0)
    struct u96 {
        u64 high;
        u32 low;
    };
    #pragma pack(pop)

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

        size_t depthMobCount{util::DivideCeil<size_t>(dimensions.depth, gobBlockDepth)}; //!< The depth of the surface in MOBs (Matrix of Blocks)
        size_t blockDepth{util::AlignUp(dimensions.depth, gobBlockDepth) - dimensions.depth};
        size_t blockPaddingZ{gobBlockDepth != 1 ? GobWidth * GobHeight * blockHeight * blockDepth : 0};

        bool hasPaddingBlock{robWidthUnalignedBytes != robWidthBytes};
        size_t blockPaddingOffset{hasPaddingBlock ? (GobWidth - (robWidthBytes - robWidthUnalignedBytes)) : 0};

        size_t pitchWidthBytes{pitchAmount ? pitchAmount : robWidthUnalignedBytes};
        size_t robBytes{pitchWidthBytes * robHeight};
        size_t gobYOffset{pitchWidthBytes * GobHeight};
        size_t gobZOffset{pitchWidthBytes * surfaceHeightLines};

        u8 *sector{blockLinear};

        auto deswizzleRob{[&](u8 *pitchRob, auto isLastRob, size_t depthSliceCount, size_t blockPaddingY = 0, size_t blockExtentY = 0) {
            auto deswizzleBlock{[&](u8 *pitchBlock, auto copySector) __attribute__((always_inline)) {
                for (size_t gobZ{}; gobZ < depthSliceCount; gobZ++) { // Every Block contains `depthSliceCount` slices, excluding padding
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

                if (depthSliceCount != gobBlockDepth) [[unlikely]]
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
                    if (xT < blockPaddingOffset) {
                        size_t copyAmount{std::min<size_t>(blockPaddingOffset - xT, SectorWidth)};

                        if constexpr (BlockLinearToPitch)
                            std::memcpy(linearSector, sector, copyAmount);
                        else
                            std::memcpy(sector, linearSector, copyAmount);
                    }
                    sector += SectorWidth;
                });
        }};

        for (size_t currMob{}; currMob < depthMobCount; ++currMob, pitch += gobZOffset * gobBlockDepth) {
            u8 *pitchRob{pitch};
            size_t sliceCount{(currMob + 1) == depthMobCount ? gobBlockDepth - blockDepth : gobBlockDepth};
            for (size_t rob{}; rob < surfaceHeightRobs; rob++) { // Every Surface contains `surfaceHeightRobs` ROBs (excl. padding ROB)
                deswizzleRob(pitchRob, std::false_type{}, sliceCount);
                pitchRob += robBytes; // Increment the linear ROB to the next ROB
            }

            if (surfaceHeightLines % robHeight != 0) {
                blockHeight = (util::AlignUp(surfaceHeightLines, GobHeight) - (surfaceHeightRobs * robHeight)) / GobHeight; // Calculate the amount of Y GOBs which aren't padding

                deswizzleRob(
                    pitchRob,
                    std::true_type{},
                    sliceCount,
                    (gobBlockHeight - blockHeight) * (GobWidth * GobHeight), // Calculate padding at the end of a block to skip
                    util::IsAligned(surfaceHeightLines, GobHeight) ? GobHeight : surfaceHeightLines - util::AlignDown(surfaceHeightLines, GobHeight) // Calculate the line relative to the start of the last GOB that is the cut-off point for the image
                );

                blockHeight = gobBlockHeight;
            }
        }
    }

    /**
     * @brief Copies pixel data between a pitch and part of a blocklinear texture
     * @tparam BlockLinearToPitch Whether to copy from a part of a blocklinear texture to a pitch texture or a pitch texture to a part of a blocklinear texture
     * @note The function assumes that the pitch texture is always equal or smaller than the blocklinear texture
     */
    template<bool BlockLinearToPitch>
    void CopyBlockLinearSubrectInternal(Dimensions pitchDimensions, Dimensions blockLinearDimensions,
                                        size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, u32 pitchAmount,
                                        size_t gobBlockHeight, size_t gobBlockDepth,
                                        u8 *blockLinear, u8 *pitch,
                                        u32 originX, u32 originY) {
        size_t pitchTextureWidth{util::DivideCeil<size_t>(pitchDimensions.width, formatBlockWidth)};
        size_t pitchTextureWidthBytes{pitchTextureWidth * formatBpb};
        size_t blockLinearTextureWidthAlignedBytes{util::AlignUp(util::DivideCeil<size_t>(blockLinearDimensions.width, formatBlockWidth) * formatBpb, GobWidth)};

        originX = util::DivideCeil<u32>(originX, static_cast<u32>(formatBlockWidth));
        size_t originXBytes{originX * formatBpb};

        if (formatBpb != 12) [[likely]] {
            size_t startingBlockXBytes{util::AlignUp(originXBytes, GobWidth) - originXBytes};
            while (formatBpb != 16) {
                if (util::IsAligned(pitchTextureWidthBytes - startingBlockXBytes, formatBpb << 1)
                && util::IsAligned(startingBlockXBytes, formatBpb << 1)) {
                    pitchTextureWidth /= 2;
                    formatBpb <<= 1;
                } else {
                    break;
                }
            }
        } else {
            formatBpb = 4;
            pitchTextureWidth *= 3;
        }

        size_t pitchTextureHeight{util::DivideCeil<size_t>(pitchDimensions.height, formatBlockHeight)};
        size_t robHeight{gobBlockHeight * GobHeight};

        originY = util::DivideCeil<u32>(originY, static_cast<u32>(formatBlockHeight));

        size_t depthMobCount{util::DivideCeil<size_t>(blockLinearDimensions.depth, gobBlockDepth)};  //!< The depth of the surface in MOBs (Matrix of Blocks)
        size_t lastMobSliceCount{gobBlockDepth - (util::AlignUp(blockLinearDimensions.depth, gobBlockDepth) - blockLinearDimensions.depth)};

        size_t pitchBytes{pitchAmount ? pitchAmount : pitchTextureWidthBytes};

        size_t robSize{blockLinearTextureWidthAlignedBytes * robHeight * gobBlockDepth};
        size_t robPerMob{util::DivideCeil<size_t>(util::DivideCeil<size_t>(blockLinearDimensions.height, formatBlockHeight), robHeight)};
        size_t blockSize{robHeight * GobWidth * gobBlockDepth};

        u8 *pitchOffset{pitch};

        auto copyTexture{[&]<typename FORMATBPB>() __attribute__((always_inline)) {
            for (size_t currMob{}; currMob < depthMobCount; ++currMob, blockLinear += robSize * robPerMob) {
                size_t sliceCount{(currMob + 1) == depthMobCount ? lastMobSliceCount : gobBlockDepth};
                u64 sliceOffset{};
                for (size_t slice{}; slice < sliceCount; ++slice, sliceOffset += (GobHeight * GobWidth * gobBlockHeight)) {
                    u64 robOffset{util::AlignDown(originY, robHeight) * blockLinearTextureWidthAlignedBytes * gobBlockDepth};
                    for (size_t line{}; line < pitchTextureHeight; ++line, pitchOffset += pitchBytes) {
                        // XYZ Offset in entire ROBs
                        if (line && !((originY + line) & (robHeight - 1))) [[unlikely]]
                            robOffset += robSize;
                        // Y Offset in entire GOBs in current block
                        size_t GobYOffset{util::AlignDown((originY + line) & (robHeight - 1), GobHeight) * GobWidth};
                        // Y Offset inside current GOB
                        GobYOffset += (((originY + line) & 0x6) << 5) + (((originY + line) & 0x1) << 4);

                        u8 *deSwizzledOffset{pitchOffset};
                        u8 *swizzledYZOffset{blockLinear + robOffset + GobYOffset + sliceOffset};
                        u8 *swizzledOffset{};

                        size_t xBytes{originXBytes};
                        size_t GobXOffset{};

                        size_t blockOffset{util::AlignDown(xBytes, GobWidth) * robHeight * gobBlockDepth};

                        for (size_t pixel{}; pixel < pitchTextureWidth; ++pixel, deSwizzledOffset += formatBpb, xBytes += formatBpb) {
                            // XYZ Offset in entire blocks in current ROB
                            if (pixel && !(xBytes & 0x3F)) [[unlikely]]
                                blockOffset += blockSize;

                            // X Offset inside current GOB
                            GobXOffset = ((xBytes & 0x20) << 3) + (xBytes & 0xF) + ((xBytes & 0x10) << 1);

                            swizzledOffset = swizzledYZOffset + blockOffset + GobXOffset;

                            if constexpr (BlockLinearToPitch)
                                *reinterpret_cast<FORMATBPB *>(deSwizzledOffset) = *reinterpret_cast<FORMATBPB *>(swizzledOffset);
                            else
                                *reinterpret_cast<FORMATBPB *>(swizzledOffset) = *reinterpret_cast<FORMATBPB *>(deSwizzledOffset);
                        }
                    }
                }
            }
        }};

        switch (formatBpb) {
            case 1: {
                copyTexture.template operator()<u8>();
                break;
            }
            case 2: {
                copyTexture.template operator()<u16>();
                break;
            }
            case 4: {
                copyTexture.template operator()<u32>();
                break;
            }
            case 8: {
                copyTexture.template operator()<u64>();
                break;
            }
            case 16: {
                copyTexture.template operator()<u128>();
                break;
            }
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

    void CopyBlockLinearToPitchSubrect(Dimensions pitchDimensions, Dimensions blockLinearDimensions,
                                       size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, u32 pitchAmount,
                                       size_t gobBlockHeight, size_t gobBlockDepth,
                                       u8 *blockLinear, u8 *pitch,
                                       u32 originX, u32 originY) {
        CopyBlockLinearSubrectInternal<true>(pitchDimensions, blockLinearDimensions,
                                             formatBlockWidth, formatBlockHeight, formatBpb, pitchAmount,
                                             gobBlockHeight, gobBlockDepth,
                                             blockLinear, pitch,
                                             originX, originY
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

    void CopyLinearToBlockLinearSubrect(Dimensions linearDimensions, Dimensions blockLinearDimensions,
                                       size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb,
                                       size_t gobBlockHeight, size_t gobBlockDepth,
                                       u8 *linear, u8 *blockLinear,
                                       u32 originX, u32 originY) {
        CopyBlockLinearSubrectInternal<false>(linearDimensions, blockLinearDimensions,
                                              formatBlockWidth, formatBlockHeight,
                                              formatBpb, 0,
                                              gobBlockHeight, gobBlockDepth,
                                              blockLinear, linear,
                                              originX, originY
        );
    }

    void CopyPitchToBlockLinearSubrect(Dimensions pitchDimensions, Dimensions blockLinearDimensions,
                                       size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, u32 pitchAmount,
                                       size_t gobBlockHeight, size_t gobBlockDepth,
                                       u8 *pitch, u8 *blockLinear,
                                       u32 originX, u32 originY) {
        CopyBlockLinearSubrectInternal<false>(pitchDimensions, blockLinearDimensions,
                                              formatBlockWidth, formatBlockHeight,
                                              formatBpb, pitchAmount,
                                              gobBlockHeight, gobBlockDepth,
                                              blockLinear, pitch,
                                              originX, originY
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
