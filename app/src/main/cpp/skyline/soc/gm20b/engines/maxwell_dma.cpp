// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 yuzu Emulator Project (https://github.com/yuzu-emu/yuzu/)

#include <gpu/interconnect/command_executor.h>
#include <gpu/texture/format.h>
#include <gpu/texture/layout.h>
#include <soc.h>
#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include "maxwell_dma.h"

namespace skyline::soc::gm20b::engine {
    MaxwellDma::MaxwellDma(const DeviceState &state, ChannelContext &channelCtx, gpu::interconnect::CommandExecutor &executor)
        : channelCtx(channelCtx), syncpoints(state.soc->host1x.syncpoints), executor(executor) {}

    __attribute__((always_inline)) void MaxwellDma::CallMethod(u32 method, u32 argument) {
        Logger::Verbose("Called method in Maxwell DMA: 0x{:X} args: 0x{:X}", method, argument);

        HandleMethod(method, argument);
    }

    void MaxwellDma::HandleMethod(u32 method, u32 argument) {
        registers.raw[method] = argument;

        if (method == ENGINE_OFFSET(launchDma))
            LaunchDma();
    }

    void MaxwellDma::LaunchDma() {
        if (*registers.lineLengthIn == 0)
            return; // Nothing to copy

        if (registers.launchDma->remapEnable) {
            Logger::Warn("DMA remapping is unimplemented!");
            return;
        }

        executor.Execute();
        if (registers.launchDma->multiLineEnable) {
            if (registers.launchDma->srcMemoryLayout == Registers::LaunchDma::MemoryLayout::Pitch &&
                  registers.launchDma->dstMemoryLayout == Registers::LaunchDma::MemoryLayout::BlockLinear)
                CopyPitchToBlockLinear();
            else if (registers.launchDma->srcMemoryLayout == Registers::LaunchDma::MemoryLayout::BlockLinear &&
                registers.launchDma->dstMemoryLayout == Registers::LaunchDma::MemoryLayout::Pitch)
                CopyBlockLinearToPitch();
            else
                Logger::Warn("Unimplemented multi-line copy type: {} -> {}!",
                              static_cast<u8>(registers.launchDma->srcMemoryLayout), static_cast<u8>(registers.launchDma->dstMemoryLayout));
        } else {
            // 1D buffer copy
            // TODO: implement swizzled 1D copies based on VMM 'kind'
            Logger::Debug("src: 0x{:X} dst: 0x{:X} size: 0x{:X}", u64{*registers.offsetIn}, u64{*registers.offsetOut}, *registers.lineLengthIn);
            channelCtx.asCtx->gmmu.Copy(*registers.offsetOut, *registers.offsetIn, *registers.lineLengthIn);
        }
    }

    void MaxwellDma::CopyPitchToBlockLinear() {
        if (registers.dstSurface->blockSize.Depth() > 1 || registers.dstSurface->depth > 1) {
            Logger::Warn("3D DMA engine copies are unimplemented!");
            return;
        }

        if (registers.dstSurface->blockSize.Width() != 1) {
            Logger::Warn("DMA engine copies with block widths other than 1 are unimplemented!");
            return;
        }

        u32 bytesPerPixel{static_cast<u32>(registers.remapComponents->ComponentSize() * registers.remapComponents->NumSrcComponents())};
        if (bytesPerPixel * *registers.lineLengthIn != *registers.pitchIn) {
            Logger::Warn("Non-linear DMA source textures are not implemented!");
            return;
        }

        if (registers.dstSurface->origin.x || registers.dstSurface->origin.y) {
            Logger::Warn("Non-zero origin DMA copies are not implemented!");
            return;
        }

        if (*registers.lineLengthIn != registers.dstSurface->width)
            Logger::Warn("DMA copy width mismatch: src: {} dst: {}", *registers.lineLengthIn, registers.dstSurface->width);

        gpu::GuestTexture srcTexture{span<u8>{},
                                     gpu::texture::Dimensions{*registers.lineLengthIn, *registers.lineCount, 1},
                                     gpu::format::GetFormatForBpp(bytesPerPixel),
                                     gpu::texture::TileConfig{ .mode = gpu::texture::TileMode::Linear },
                                     gpu::texture::TextureType::e2D};

        if (auto mappings{channelCtx.asCtx->gmmu.TranslateRange(*registers.offsetIn, srcTexture.GetLayerStride())}; mappings.size() == 1) {
            srcTexture.mappings[0] = mappings[0];
        } else {
            Logger::Warn("DMA for split textures is unimplemented!");
            return;
        }

        // This represents a single layer view into a potentially multi-layer texture
        gpu::GuestTexture dstTexture{span<u8>{},
                                     gpu::texture::Dimensions{*registers.lineLengthIn, registers.dstSurface->height, 1},
                                     gpu::format::GetFormatForBpp(bytesPerPixel),
                                     gpu::texture::TileConfig{ .mode = gpu::texture::TileMode::Block, .blockHeight = registers.dstSurface->blockSize.Height(), .blockDepth = 1 },
                                     gpu::texture::TextureType::e2D};

        u64 dstLayerAddress{*registers.offsetOut + dstTexture.GetLayerStride() * registers.dstSurface->layer};
        if (auto mappings{channelCtx.asCtx->gmmu.TranslateRange(dstLayerAddress, dstTexture.GetLayerStride())}; mappings.size() == 1) {
            dstTexture.mappings[0] = mappings[0];
        } else {
            Logger::Warn("DMA for split textures is unimplemented!");
            return;
        }

        Logger::Debug("{}x{}@0x{:X} -> {}x{}@0x{:X}", srcTexture.dimensions.width, srcTexture.dimensions.height, u64{*registers.offsetIn}, dstTexture.dimensions.width, dstTexture.dimensions.height, dstLayerAddress);

        gpu::texture::CopyLinearToBlockLinear(dstTexture, srcTexture.mappings.front().data(), dstTexture.mappings.front().data());
    }


    void MaxwellDma::CopyBlockLinearToPitch() {
        if (registers.srcSurface->blockSize.Depth() > 1 || registers.srcSurface->depth > 1) {
            Logger::Warn("3D DMA engine copies are unimplemented!");
            return;
        }

        if (registers.srcSurface->blockSize.Width() != 1) {
            Logger::Warn("DMA engine copies with block widths other than 1 are unimplemented!");
            return;
        }

        u32 bytesPerPixel{static_cast<u32>(registers.remapComponents->ComponentSize() * registers.remapComponents->NumSrcComponents())};
        if (bytesPerPixel * *registers.lineLengthIn != *registers.pitchOut) {
            Logger::Warn("Non-linear DMA destination textures are not implemented!");
            return;
        }

        if (registers.srcSurface->origin.x || registers.srcSurface->origin.y) {
            Logger::Warn("Non-zero origin DMA copies are not implemented!");
            return;
        }

        if (*registers.lineLengthIn != registers.srcSurface->width)
            Logger::Warn("DMA copy width mismatch: src: {} dst: {}", *registers.lineLengthIn, registers.dstSurface->width);

        gpu::GuestTexture srcTexture{span<u8>{},
                                     gpu::texture::Dimensions{registers.srcSurface->width, registers.srcSurface->height, 1},
                                     gpu::format::GetFormatForBpp(bytesPerPixel),
                                     gpu::texture::TileConfig{ .mode = gpu::texture::TileMode::Block, .blockHeight = registers.srcSurface->blockSize.Height(), .blockDepth = 1 },
                                     gpu::texture::TextureType::e2D};

        if (auto mappings{channelCtx.asCtx->gmmu.TranslateRange(*registers.offsetIn, srcTexture.GetLayerStride())}; mappings.size() == 1) {
            srcTexture.mappings[0] = mappings[0];
        } else {
            Logger::Warn("DMA for split textures is unimplemented!");
            return;
        }

        gpu::GuestTexture dstTexture{span<u8>{},
                                     gpu::texture::Dimensions{*registers.lineLengthIn, *registers.lineCount, 1},
                                     gpu::format::GetFormatForBpp(bytesPerPixel),
                                     gpu::texture::TileConfig{ .mode = gpu::texture::TileMode::Linear },
                                     gpu::texture::TextureType::e2D};

        if (auto mappings{channelCtx.asCtx->gmmu.TranslateRange(*registers.offsetOut, dstTexture.GetLayerStride())}; mappings.size() == 1) {
            dstTexture.mappings[0] = mappings[0];
        } else {
            Logger::Warn("DMA for split textures is unimplemented!");
            return;
        }

        Logger::Debug("{}x{}@0x{:X} -> {}x{}@0x{:X}", srcTexture.dimensions.width, srcTexture.dimensions.height, u64{*registers.offsetIn}, dstTexture.dimensions.width, dstTexture.dimensions.height,  u64{*registers.offsetOut});

        gpu::texture::CopyBlockLinearToLinear(srcTexture, srcTexture.mappings.front().data(), dstTexture.mappings.front().data());
    }

    void MaxwellDma::CallMethodBatchNonInc(u32 method, span<u32> arguments) {
        for (u32 argument : arguments)
            HandleMethod(method, argument);
    }
}
