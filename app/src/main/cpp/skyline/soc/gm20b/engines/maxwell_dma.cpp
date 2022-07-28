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

        executor.Submit();
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

        ReleaseSemaphore();
    }

    void MaxwellDma::ReleaseSemaphore() {
        if (registers.launchDma->reductionEnable)
            Logger::Warn("Semaphore reduction is unimplemented!");

        u64 address{registers.semaphore->address};
        u64 payload{registers.semaphore->payload};
        switch (registers.launchDma->semaphoreType) {
            case Registers::LaunchDma::SemaphoreType::ReleaseOneWordSemaphore:
                channelCtx.asCtx->gmmu.Write(address, registers.semaphore->payload);
                Logger::Debug("address: 0x{:X} payload: {}", address, payload);
                break;
            case Registers::LaunchDma::SemaphoreType::ReleaseFourWordSemaphore: {
                // Write timestamp first to ensure correct ordering
                u64 timestamp{GetGpuTimeTicks()};
                channelCtx.asCtx->gmmu.Write(address + 8, timestamp);
                channelCtx.asCtx->gmmu.Write(address, static_cast<u64>(payload));
                Logger::Debug("address: 0x{:X} payload: {} timestamp: {}", address, payload, timestamp);
                break;
            }
            default:
                break;
        }
    }

    void MaxwellDma::CopyPitchToBlockLinear() {
        if (registers.dstSurface->blockSize.Depth() > 1 || registers.dstSurface->depth > 1) {
            Logger::Warn("3D DMA engine copies are unimplemented");
            return;
        }

        if (registers.dstSurface->blockSize.Width() != 1) {
            Logger::Warn("DMA engine copies with block widths other than 1 are unimplemented");
            return;
        }

        u32 bytesPerPixel{static_cast<u32>(registers.remapComponents->ComponentSize() * registers.remapComponents->NumSrcComponents())};
        if (bytesPerPixel * *registers.lineLengthIn != *registers.pitchIn) {
            Logger::Warn("Non-linear DMA source textures are not implemented");
            return;
        }

        if (registers.dstSurface->origin.x || registers.dstSurface->origin.y) {
            Logger::Warn("Non-zero origin DMA copies are not implemented");
            return;
        }

        if (*registers.lineLengthIn != registers.dstSurface->width)
            Logger::Warn("DMA copy width mismatch: src: {} dst: {}", *registers.lineLengthIn, registers.dstSurface->width);

        gpu::texture::Dimensions srcDimensions{*registers.lineLengthIn, *registers.lineCount, 1};
        size_t srcStride{srcDimensions.width * srcDimensions.height * bytesPerPixel};

        auto srcMappings{channelCtx.asCtx->gmmu.TranslateRange(*registers.offsetIn, srcStride)};
        if (srcMappings.size() != 1) {
            Logger::Warn("DMA for split textures is unimplemented");
            return;
        }

        gpu::texture::Dimensions dstDimensions{registers.dstSurface->width, registers.dstSurface->height, registers.dstSurface->depth};
        dstDimensions.width = *registers.lineLengthIn; // We do not support copying subrects so we need the width to match on the source and destination
        size_t dstBlockHeight{registers.dstSurface->blockSize.Height()}, dstBlockDepth{registers.dstSurface->blockSize.Depth()};
        size_t dstLayerStride{gpu::texture::GetBlockLinearLayerSize(dstDimensions, 1, 1, bytesPerPixel, dstBlockHeight, dstBlockDepth)};

        size_t dstLayerAddress{*registers.offsetOut + (registers.dstSurface->layer * dstLayerStride)};
        auto dstMappings{channelCtx.asCtx->gmmu.TranslateRange(dstLayerAddress, dstLayerStride)};
        if (dstMappings.size() != 1) {
            Logger::Warn("DMA for split textures is unimplemented");
            return;
        }

        Logger::Debug("{}x{}@0x{:X} -> {}x{}@0x{:X}", srcDimensions.width, srcDimensions.height, u64{*registers.offsetIn}, dstDimensions.width, dstDimensions.height, dstLayerAddress);

        gpu::texture::CopyLinearToBlockLinear(
            dstDimensions,
            1, 1, bytesPerPixel,
            dstBlockHeight, dstBlockDepth,
            srcMappings.front().data(), dstMappings.front().data()
        );
    }

    void MaxwellDma::CopyBlockLinearToPitch() {
        if (registers.srcSurface->blockSize.Depth() > 1 || registers.srcSurface->depth > 1) {
            Logger::Warn("3D DMA engine copies are unimplemented");
            return;
        }

        if (registers.srcSurface->blockSize.Width() != 1) {
            Logger::Warn("DMA engine copies with block widths other than 1 are unimplemented");
            return;
        }

        u32 bytesPerPixel{static_cast<u32>(registers.remapComponents->ComponentSize() * registers.remapComponents->NumSrcComponents())};
        if (bytesPerPixel * *registers.lineLengthIn != *registers.pitchOut) {
            Logger::Warn("Non-linear DMA destination textures are not implemented");
            return;
        }

        if (registers.srcSurface->origin.x || registers.srcSurface->origin.y) {
            Logger::Warn("Non-zero origin DMA copies are not implemented");
            return;
        }

        if (*registers.lineLengthIn != registers.srcSurface->width)
            Logger::Warn("DMA copy width mismatch: src: {} dst: {}", *registers.lineLengthIn, registers.dstSurface->width);

        gpu::texture::Dimensions srcDimensions{registers.srcSurface->width, registers.srcSurface->height, registers.srcSurface->depth};
        srcDimensions.width = *registers.lineLengthIn; // We do not support copying subrects so we need the width to match on the source and destination
        size_t srcBlockHeight{registers.srcSurface->blockSize.Height()}, srcBlockDepth{registers.srcSurface->blockSize.Depth()};
        size_t srcStride{gpu::texture::GetBlockLinearLayerSize(srcDimensions, 1, 1, bytesPerPixel, srcBlockHeight, srcBlockDepth)};

        auto srcMappings{channelCtx.asCtx->gmmu.TranslateRange(*registers.offsetIn, srcStride)};
        if (srcMappings.size() != 1) {
            Logger::Warn("DMA for split textures is unimplemented");
            return;
        }

        gpu::texture::Dimensions dstDimensions{*registers.lineLengthIn, *registers.lineCount, 1};
        size_t dstStride{dstDimensions.width * dstDimensions.height * bytesPerPixel};

        auto dstMappings{channelCtx.asCtx->gmmu.TranslateRange(*registers.offsetOut, dstStride)};
        if (dstMappings.size() != 1) {
            Logger::Warn("DMA for split textures is unimplemented");
            return;
        }

        Logger::Debug("{}x{}@0x{:X} -> {}x{}@0x{:X}", srcDimensions.width, srcDimensions.height, u64{*registers.offsetIn}, dstDimensions.width, dstDimensions.height, u64{*registers.offsetOut});

        gpu::texture::CopyBlockLinearToLinear(
            srcDimensions,
            1, 1, bytesPerPixel,
            srcBlockHeight, srcBlockDepth,
            srcMappings.front().data(), dstMappings.front().data());
    }

    void MaxwellDma::CallMethodBatchNonInc(u32 method, span<u32> arguments) {
        for (u32 argument : arguments)
            HandleMethod(method, argument);
    }
}
