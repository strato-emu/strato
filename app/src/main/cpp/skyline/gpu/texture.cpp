// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/native_window.h>
#include <kernel/types/KProcess.h>
#include "texture.h"

namespace skyline::gpu {
    GuestTexture::GuestTexture(const DeviceState &state, u64 address, texture::Dimensions dimensions, texture::Format format, texture::TileMode tiling, texture::TileConfig layout) : state(state), address(address), dimensions(dimensions), format(format), tileMode(tiling), tileConfig(layout) {}

    Texture::Texture(const DeviceState &state, std::shared_ptr<GuestTexture> guest, texture::Dimensions dimensions, texture::Format format, texture::Swizzle swizzle) : state(state), guest(guest), dimensions(dimensions), format(format), swizzle(swizzle) {
        SynchronizeHost();
    }

    void Texture::SynchronizeHost() {
        auto texture = state.process->GetPointer<u8>(guest->address);
        auto size = format.GetSize(dimensions);
        backing.resize(size);
        auto output = reinterpret_cast<u8 *>(backing.data());

        if (guest->tileMode == texture::TileMode::Block) {
            // Reference on Block-linear tiling: https://gist.github.com/PixelyIon/d9c35050af0ef5690566ca9f0965bc32
            constexpr u8 sectorWidth = 16; // The width of a sector in bytes
            constexpr u8 sectorHeight = 2; // The height of a sector in lines
            constexpr u8 gobWidth = 64; // The width of a GOB in bytes
            constexpr u8 gobHeight = 8; // The height of a GOB in lines

            auto robHeight = gobHeight * guest->tileConfig.blockHeight; // The height of a single ROB (Row of Blocks) in lines
            auto surfaceHeightRobs = util::AlignUp(dimensions.height / format.blockHeight, robHeight) / robHeight; // The height of the surface in ROBs (Row Of Blocks)
            auto robWidthBytes = util::AlignUp((guest->tileConfig.surfaceWidth / format.blockWidth) * format.bpb, gobWidth); // The width of a ROB in bytes
            auto robWidthBlocks = robWidthBytes / gobWidth; // The width of a ROB in blocks (and GOBs because block width == 1 on the Tegra X1)
            auto robBytes = robWidthBytes * robHeight; // The size of a ROB in bytes
            auto gobYOffset = robWidthBytes * gobHeight; // The offset of the next Y-axis GOB from the current one in linear space

            auto inputSector = texture; // The address of the input sector
            auto outputRob = output; // The address of the output block

            for (u32 rob = 0; rob < surfaceHeightRobs; rob++) { // Every Surface contains `surfaceHeightRobs` ROBs
                auto outputBlock = outputRob; // We iterate through a block independently of the ROB
                for (u32 block = 0; block < robWidthBlocks; block++) { // Every ROB contains `surfaceWidthBlocks` Blocks
                    auto outputGob = outputBlock; // We iterate through a GOB independently of the block
                    for (u32 gobY = 0; gobY < guest->tileConfig.blockHeight; gobY++) { // Every Block contains `blockHeight` Y-axis GOBs
                        for (u32 index = 0; index < sectorWidth * sectorHeight; index++) { // Every Y-axis GOB contains `sectorWidth * sectorHeight` sectors
                            u32 xT = ((index << 3) & 0b10000) | ((index << 1) & 0b100000); // Morton-Swizzle on the X-axis
                            u32 yT = ((index >> 1) & 0b110) | (index & 0b1); // Morton-Swizzle on the Y-axis
                            std::memcpy(outputGob + (yT * robWidthBytes) + xT, inputSector, sectorWidth);
                            inputSector += sectorWidth; // `sectorWidth` bytes are of sequential image data
                        }
                        outputGob += gobYOffset; // Increment the output GOB to the next Y-axis GOB
                    }
                    outputBlock += gobWidth; // Increment the output block to the next block (As Block Width = 1 GOB Width)
                }
                outputRob += robBytes; // Increment the output block to the next ROB
            }
        } else if (guest->tileMode == texture::TileMode::Pitch) {
            auto sizeLine = guest->format.GetSize(dimensions.width, 1); // The size of a single line of pixel data
            auto sizeStride = guest->format.GetSize(guest->tileConfig.pitch, 1); // The size of a single stride of pixel data

            auto inputLine = texture; // The address of the input line
            auto outputLine = output; // The address of the output line

            for (u32 line = 0; line < dimensions.height; line++) {
                std::memcpy(outputLine, inputLine, sizeLine);
                inputLine += sizeStride;
                outputLine += sizeLine;
            }
        } else if (guest->tileMode == texture::TileMode::Linear) {
            std::memcpy(output, texture, size);
        }
    }

    PresentationTexture::PresentationTexture(const DeviceState &state, const std::shared_ptr<GuestTexture> &guest, const texture::Dimensions &dimensions, const texture::Format &format, const std::function<void()> &releaseCallback) : releaseCallback(releaseCallback), Texture(state, guest, dimensions, format, {}) {}

    i32 PresentationTexture::GetAndroidFormat() {
        switch (format.vkFormat) {
            case vk::Format::eR8G8B8A8Unorm:
                return WINDOW_FORMAT_RGBA_8888;
            case vk::Format::eR5G6B5UnormPack16:
                return WINDOW_FORMAT_RGB_565;
            default:
                throw exception("GetAndroidFormat: Cannot find corresponding Android surface format");
        }
    }
}
