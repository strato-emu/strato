// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "texture.h"

namespace skyline::gpu {
    /**
     * @brief Copies the contents of a blocklinear guest texture to a linear output buffer
     */
    void CopyBlockLinearToLinear(GuestTexture& guest, u8* guestInput, u8* linearOutput) {
        // Reference on Block-linear tiling: https://gist.github.com/PixelyIon/d9c35050af0ef5690566ca9f0965bc32
        constexpr u8 SectorWidth{16}; // The width of a sector in bytes
        constexpr u8 SectorHeight{2}; // The height of a sector in lines
        constexpr u8 GobWidth{64}; // The width of a GOB in bytes
        constexpr u8 GobHeight{8}; // The height of a GOB in lines

        auto blockHeight{guest.tileConfig.blockHeight}; //!< The height of the blocks in GOBs
        auto robHeight{GobHeight * blockHeight}; //!< The height of a single ROB (Row of Blocks) in lines
        auto surfaceHeight{guest.dimensions.height / guest.format->blockHeight}; //!< The height of the surface in lines
        auto surfaceHeightRobs{util::AlignUp(surfaceHeight, robHeight) / robHeight}; //!< The height of the surface in ROBs (Row Of Blocks)
        auto robWidthBytes{util::AlignUp((guest.dimensions.width / guest.format->blockWidth) * guest.format->bpb, GobWidth)}; //!< The width of a ROB in bytes
        auto robWidthBlocks{robWidthBytes / GobWidth}; //!< The width of a ROB in blocks (and GOBs because block width == 1 on the Tegra X1)
        auto robBytes{robWidthBytes * robHeight}; //!< The size of a ROB in bytes
        auto gobYOffset{robWidthBytes * GobHeight}; //!< The offset of the next Y-axis GOB from the current one in linear space

        auto inputSector{guestInput};
        auto outputRob{linearOutput};

        for (u32 rob{}, y{}, paddingY{}; rob < surfaceHeightRobs; rob++) { // Every Surface contains `surfaceHeightRobs` ROBs
            auto outputBlock{outputRob}; // We iterate through a block independently of the ROB
            for (u32 block{}; block < robWidthBlocks; block++) { // Every ROB contains `surfaceWidthBlocks` Blocks
                auto outputGob{outputBlock}; // We iterate through a GOB independently of the block
                for (u32 gobY{}; gobY < blockHeight; gobY++) { // Every Block contains `blockHeight` Y-axis GOBs
                    for (u32 index{}; index < SectorWidth * SectorHeight; index++) { // Every Y-axis GOB contains `sectorWidth * sectorHeight` sectors
                        u32 xT{((index << 3) & 0b10000) | ((index << 1) & 0b100000)}; // Morton-Swizzle on the X-axis
                        u32 yT{((index >> 1) & 0b110) | (index & 0b1)}; // Morton-Swizzle on the Y-axis
                        std::memcpy(outputGob + (yT * robWidthBytes) + xT, inputSector, SectorWidth);
                        inputSector += SectorWidth; // `sectorWidth` bytes are of sequential image data
                    }
                    outputGob += gobYOffset; // Increment the output GOB to the next Y-axis GOB
                }
                inputSector += paddingY; // Increment the input sector to the next sector
                outputBlock += GobWidth; // Increment the output block to the next block (As Block Width = 1 GOB Width)
            }
            outputRob += robBytes; // Increment the output block to the next ROB

            y += robHeight; // Increment the Y position to the next ROB
            blockHeight = static_cast<u8>(std::min(static_cast<u32>(blockHeight), (surfaceHeight - y) / GobHeight)); // Calculate the amount of Y GOBs which aren't padding
            paddingY = (guest.tileConfig.blockHeight - blockHeight) * (SectorWidth * SectorWidth * SectorHeight); // Calculate the amount of padding between contiguous sectors
        }
    }
    /**
     * @brief Copies the contents of a blocklinear guest texture to a linear output buffer
     */
    void CopyLinearToBlockLinear(GuestTexture& guest, u8* linearInput, u8* guestOutput) {
        // Reference on Block-linear tiling: https://gist.github.com/PixelyIon/d9c35050af0ef5690566ca9f0965bc32
        constexpr u8 SectorWidth{16}; // The width of a sector in bytes
        constexpr u8 SectorHeight{2}; // The height of a sector in lines
        constexpr u8 GobWidth{64}; // The width of a GOB in bytes
        constexpr u8 GobHeight{8}; // The height of a GOB in lines

        auto blockHeight{guest.tileConfig.blockHeight}; //!< The height of the blocks in GOBs
        auto robHeight{GobHeight * blockHeight}; //!< The height of a single ROB (Row of Blocks) in lines
        auto surfaceHeight{guest.dimensions.height / guest.format->blockHeight}; //!< The height of the surface in lines
        auto surfaceHeightRobs{util::AlignUp(surfaceHeight, robHeight) / robHeight}; //!< The height of the surface in ROBs (Row Of Blocks)
        auto robWidthBytes{util::AlignUp((guest.dimensions.width / guest.format->blockWidth) * guest.format->bpb, GobWidth)}; //!< The width of a ROB in bytes
        auto robWidthBlocks{robWidthBytes / GobWidth}; //!< The width of a ROB in blocks (and GOBs because block width == 1 on the Tegra X1)
        auto robBytes{robWidthBytes * robHeight}; //!< The size of a ROB in bytes
        auto gobYOffset{robWidthBytes * GobHeight}; //!< The offset of the next Y-axis GOB from the current one in linear space

        auto outputSector{guestOutput};
        auto inputRob{linearInput};

        for (u32 rob{}, y{}, paddingY{}; rob < surfaceHeightRobs; rob++) { // Every Surface contains `surfaceHeightRobs` ROBs
            auto outputBlock{inputRob}; // We iterate through a block independently of the ROB
            for (u32 block{}; block < robWidthBlocks; block++) { // Every ROB contains `surfaceWidthBlocks` Blocks
                auto inputGob{outputBlock}; // We iterate through a GOB independently of the block
                for (u32 gobY{}; gobY < blockHeight; gobY++) { // Every Block contains `blockHeight` Y-axis GOBs
                    for (u32 index{}; index < SectorWidth * SectorHeight; index++) { // Every Y-axis GOB contains `sectorWidth * sectorHeight` sectors
                        u32 xT{((index << 3) & 0b10000) | ((index << 1) & 0b100000)}; // Morton-Swizzle on the X-axis
                        u32 yT{((index >> 1) & 0b110) | (index & 0b1)}; // Morton-Swizzle on the Y-axis
                        std::memcpy(outputSector, inputGob + (yT * robWidthBytes) + xT, SectorWidth);
                        outputSector += SectorWidth; // `sectorWidth` bytes are of sequential image data
                    }
                    inputGob += gobYOffset; // Increment the output GOB to the next Y-axis GOB
                }
                outputSector += paddingY; // Increment the input sector to the next sector
                outputBlock += GobWidth; // Increment the output block to the next block (As Block Width = 1 GOB Width)
            }
            inputRob += robBytes; // Increment the output block to the next ROB

            y += robHeight; // Increment the Y position to the next ROB
            blockHeight = static_cast<u8>(std::min(static_cast<u32>(blockHeight), (surfaceHeight - y) / GobHeight)); // Calculate the amount of Y GOBs which aren't padding
            paddingY = (guest.tileConfig.blockHeight - blockHeight) * (SectorWidth * SectorWidth * SectorHeight); // Calculate the amount of padding between contiguous sectors
        }
    }

    /**
     * @brief Copies the contents of a pitch-linear guest texture to a linear output buffer
     */
    void CopyPitchLinearToLinear(GuestTexture& guest, u8* guestInput, u8* linearOutput) {
        auto sizeLine{guest.format->GetSize(guest.dimensions.width, 1)}; //!< The size of a single line of pixel data
        auto sizeStride{guest.format->GetSize(guest.tileConfig.pitch, 1)}; //!< The size of a single stride of pixel data

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
    void CopyLinearToPitchLinear(GuestTexture& guest, u8* linearInput, u8* guestOutput) {
        auto sizeLine{guest.format->GetSize(guest.dimensions.width, 1)}; //!< The size of a single line of pixel data
        auto sizeStride{guest.format->GetSize(guest.tileConfig.pitch, 1)}; //!< The size of a single stride of pixel data

        auto inputLine{linearInput};
        auto outputLine{guestOutput};

        for (u32 line{}; line < guest.dimensions.height; line++) {
            std::memcpy(outputLine, inputLine, sizeLine);
            inputLine += sizeLine;
            outputLine += sizeStride;
        }
    }
}
