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
    MaxwellDma::MaxwellDma(const DeviceState &state, ChannelContext &channelCtx)
        : channelCtx{channelCtx},
          syncpoints{state.soc->host1x.syncpoints},
          interconnect{*state.gpu, channelCtx},
          copyCache() {}

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
        DmaCopy();

        ReleaseSemaphore();
    }

    void MaxwellDma::DmaCopy() {
        if (registers.launchDma->multiLineEnable) {
            if (registers.launchDma->remapEnable) [[unlikely]] {
                Logger::Warn("Remapped DMA copies are unimplemented!");
                return;
            }

            channelCtx.executor.Submit();

            if (registers.launchDma->srcMemoryLayout == registers.launchDma->dstMemoryLayout) [[unlikely]] {
                // Pitch to Pitch copy
                if (registers.launchDma->srcMemoryLayout == Registers::LaunchDma::MemoryLayout::Pitch) [[likely]] {
                    CopyPitchToPitch();
                } else {
                    Logger::Warn("BlockLinear to BlockLinear DMA copies are unimplemented!");
                }
            } else if (registers.launchDma->srcMemoryLayout == Registers::LaunchDma::MemoryLayout::BlockLinear) {
                CopyBlockLinearToPitch();
            } else [[likely]] {
                CopyPitchToBlockLinear();
            }
        } else {
            // 1D copy
            // TODO: implement swizzled 1D copies based on VMM 'kind'
            Logger::Debug("src: 0x{:X} dst: 0x{:X} size: 0x{:X}", u64{*registers.offsetIn}, u64{*registers.offsetOut}, *registers.lineLengthIn);

            size_t dstBpp{registers.launchDma->remapEnable ? static_cast<size_t>(registers.remapComponents->NumDstComponents() * registers.remapComponents->ComponentSize()) : 1};

            auto srcMappings{channelCtx.asCtx->gmmu.TranslateRange(*registers.offsetIn, *registers.lineLengthIn)};
            auto dstMappings{channelCtx.asCtx->gmmu.TranslateRange(*registers.offsetOut, *registers.lineLengthIn * dstBpp)};

            if (registers.launchDma->remapEnable) [[unlikely]] {
                // Remapped buffer clears
                if ((registers.remapComponents->dstX == Registers::RemapComponents::Swizzle::ConstA) &&
                    (registers.remapComponents->dstY == Registers::RemapComponents::Swizzle::ConstA) &&
                    (registers.remapComponents->dstZ == Registers::RemapComponents::Swizzle::ConstA) &&
                    (registers.remapComponents->dstW == Registers::RemapComponents::Swizzle::ConstA) &&
                    (registers.remapComponents->ComponentSize() == 4)) {
                    for (auto mapping : dstMappings)
                        interconnect.Clear(mapping, *registers.remapConstA);
                } else {
                    Logger::Warn("Remapped DMA copies are unimplemented!");
                }
            } else {
                if (srcMappings.size() != 1 || dstMappings.size() != 1) [[unlikely]]
                    channelCtx.asCtx->gmmu.Copy(u64{*registers.offsetOut}, u64{*registers.offsetIn}, *registers.lineLengthIn);
                else
                    interconnect.Copy(dstMappings.front(), srcMappings.front());
            }
        }
    }

    void MaxwellDma::HandleSplitCopy(TranslatedAddressRange srcMappings, TranslatedAddressRange dstMappings, size_t srcSize, size_t dstSize, auto copyCallback) {
        bool isSrcSplit{};
        u8 *src{srcMappings.front().data()}, *dst{dstMappings.front().data()};
        if (srcMappings.size() != 1) {
            if (copyCache.size() < srcSize)
                copyCache.resize(srcSize);

            src = copyCache.data();
            channelCtx.asCtx->gmmu.Read(src, u64{*registers.offsetIn}, srcSize);

            isSrcSplit = true;
        }
        if (dstMappings.size() != 1) {
            size_t offset{isSrcSplit ? srcSize : 0};

            if (copyCache.size() < (dstSize + offset))
                copyCache.resize(dstSize);

            dst = copyCache.data() + offset;

            // If the destination is not entirely filled by the copy we copy it's current state in the cache to prevent clearing of other data.
            if (registers.launchDma->dstMemoryLayout == Registers::LaunchDma::MemoryLayout::BlockLinear)
                channelCtx.asCtx->gmmu.Read(dst, u64{*registers.offsetOut}, dstSize);
        }

        copyCallback(src, dst);

        if (dstMappings.size() != 1)
            channelCtx.asCtx->gmmu.Write(u64{*registers.offsetOut}, dst, dstSize);
    }

    void MaxwellDma::CopyPitchToPitch() {
        auto srcMappings{channelCtx.asCtx->gmmu.TranslateRange(*registers.offsetIn, *registers.pitchIn * *registers.lineCount)};
        auto dstMappings{channelCtx.asCtx->gmmu.TranslateRange(*registers.offsetOut, *registers.pitchOut * *registers.lineCount)};

        if (srcMappings.size() != 1 || dstMappings.size() != 1) [[unlikely]] {
            HandleSplitCopy(srcMappings, dstMappings, *registers.lineLengthIn, *registers.lineLengthIn, [&](u8 *src, u8 *dst) {
                // Both Linear, copy as is.
                if ((*registers.pitchIn == *registers.pitchOut) && (*registers.pitchIn == *registers.lineLengthIn))
                    std::memcpy(dst, src, *registers.lineLengthIn * *registers.lineCount);
                else
                    for (size_t linesToCopy{*registers.lineCount}, srcCopyOffset{}, dstCopyOffset{}; linesToCopy; --linesToCopy, srcCopyOffset += *registers.pitchIn, dstCopyOffset += *registers.pitchOut)
                        std::memcpy(dst + dstCopyOffset, src + srcCopyOffset, *registers.lineLengthIn);
            });
        } else [[likely]] {
            // Both Linear, copy as is.
            if ((*registers.pitchIn == *registers.pitchOut) && (*registers.pitchIn == *registers.lineLengthIn)) {
                std::memcpy(dstMappings.front().data(), srcMappings.front().data(), *registers.lineLengthIn * *registers.lineCount);
            } else {
                for (size_t linesToCopy{*registers.lineCount}, srcCopyOffset{}, dstCopyOffset{}; linesToCopy; --linesToCopy, srcCopyOffset += *registers.pitchIn, dstCopyOffset += *registers.pitchOut)
                    std::memcpy(dstMappings.front().subspan(dstCopyOffset).data(), srcMappings.front().subspan(srcCopyOffset).data(), *registers.lineLengthIn);
            }
        }
    }

    void MaxwellDma::CopyBlockLinearToPitch() {
        if (registers.srcSurface->blockSize.Width() != 1) [[unlikely]] {
            Logger::Error("Blocklinear surfaces with a non-one block width are unsupported on the Tegra X1: {}", registers.srcSurface->blockSize.Width());
            return;
        }

        gpu::texture::Dimensions srcDimensions{registers.srcSurface->width, registers.srcSurface->height, registers.srcSurface->depth};
        size_t srcLayerStride{gpu::texture::GetBlockLinearLayerSize(srcDimensions, 1, 1, 1, registers.srcSurface->blockSize.Height(), registers.srcSurface->blockSize.Depth())};
        size_t srcLayerAddress{*registers.offsetIn + (registers.srcSurface->layer * srcLayerStride)};

        // Get source address
        auto srcMappings{channelCtx.asCtx->gmmu.TranslateRange(*registers.offsetIn, srcLayerStride)};

        gpu::texture::Dimensions dstDimensions{*registers.lineLengthIn, *registers.lineCount, registers.srcSurface->depth};
        size_t dstSize{*registers.pitchOut * dstDimensions.height * dstDimensions.depth}; // If remapping is not enabled there are only 1 bytes per pixel

        // Get destination address
        auto dstMappings{channelCtx.asCtx->gmmu.TranslateRange(*registers.offsetOut, dstSize)};

        auto copyFunc{[&](u8 *src, u8 *dst) {
            if ((util::AlignDown(srcDimensions.width, 64) != util::AlignDown(dstDimensions.width, 64))
                || registers.srcSurface->origin.x || registers.srcSurface->origin.y) {
                gpu::texture::CopyBlockLinearToPitchSubrect(
                    dstDimensions, srcDimensions,
                    1, 1, 1, *registers.pitchOut,
                    registers.srcSurface->blockSize.Height(), registers.srcSurface->blockSize.Depth(),
                    src, dst,
                    registers.srcSurface->origin.x, registers.srcSurface->origin.y
                );
            } else [[likely]] {
                gpu::texture::CopyBlockLinearToPitch(
                    dstDimensions,
                    1, 1, 1, *registers.pitchOut,
                    registers.srcSurface->blockSize.Height(), registers.srcSurface->blockSize.Depth(),
                    src, dst
                );
            }
        }};

        Logger::Debug("{}x{}x{}@0x{:X} -> {}x{}x{}@0x{:X}", srcDimensions.width, srcDimensions.height, srcDimensions.depth, srcLayerAddress, dstDimensions.width, dstDimensions.height, dstDimensions.depth, u64{*registers.offsetOut});

        if (srcMappings.size() != 1 || dstMappings.size() != 1) [[unlikely]]
            HandleSplitCopy(srcMappings, dstMappings, srcLayerStride, dstSize, copyFunc);
        else [[likely]]
            copyFunc(srcMappings.front().data(), dstMappings.front().data());
    }

    void MaxwellDma::CopyPitchToBlockLinear() {
        if (registers.dstSurface->blockSize.Width() != 1) [[unlikely]] {
            Logger::Error("Blocklinear surfaces with a non-one block width are unsupported on the Tegra X1: {}", registers.srcSurface->blockSize.Width());
            return;
        }

        gpu::texture::Dimensions srcDimensions{*registers.lineLengthIn, *registers.lineCount, registers.dstSurface->depth};
        size_t srcSize{*registers.pitchIn * srcDimensions.height * srcDimensions.depth}; // If remapping is not enabled there are only 1 bytes per pixel

        // Get source address
        auto srcMappings{channelCtx.asCtx->gmmu.TranslateRange(*registers.offsetIn, srcSize)};

        gpu::texture::Dimensions dstDimensions{registers.dstSurface->width, registers.dstSurface->height, registers.dstSurface->depth};
        size_t dstLayerStride{gpu::texture::GetBlockLinearLayerSize(dstDimensions, 1, 1, 1, registers.dstSurface->blockSize.Height(), registers.dstSurface->blockSize.Depth())};
        size_t dstLayerAddress{*registers.offsetOut + (registers.dstSurface->layer * dstLayerStride)};

        // Get destination address
        auto dstMappings{channelCtx.asCtx->gmmu.TranslateRange(*registers.offsetOut, dstLayerStride)};

        Logger::Debug("{}x{}x{}@0x{:X} -> {}x{}x{}@0x{:X}", srcDimensions.width, srcDimensions.height, srcDimensions.depth, u64{*registers.offsetIn}, dstDimensions.width, dstDimensions.height, dstDimensions.depth, dstLayerAddress);

        auto copyFunc{[&](u8 *src, u8 *dst) {
            if ((util::AlignDown(srcDimensions.width, 64) != util::AlignDown(dstDimensions.width, 64))
                || registers.dstSurface->origin.x || registers.dstSurface->origin.y) {
                gpu::texture::CopyPitchToBlockLinearSubrect(
                    srcDimensions, dstDimensions,
                    1, 1, 1, *registers.pitchIn,
                    registers.dstSurface->blockSize.Height(), registers.dstSurface->blockSize.Depth(),
                    src, dst,
                    registers.dstSurface->origin.x, registers.dstSurface->origin.y
                );
            } else [[likely]] {
                gpu::texture::CopyPitchToBlockLinear(
                    srcDimensions,
                    1, 1, 1, *registers.pitchIn,
                    registers.dstSurface->blockSize.Height(), registers.dstSurface->blockSize.Depth(),
                    src, dst
                );
            }
        }};

        if (srcMappings.size() != 1 || dstMappings.size() != 1) [[unlikely]]
            HandleSplitCopy(srcMappings, dstMappings, srcSize, dstLayerStride, copyFunc);
        else [[likely]]
            copyFunc(srcMappings.front().data(), dstMappings.front().data());
    }

    void MaxwellDma::ReleaseSemaphore() {
        if (registers.launchDma->reductionEnable) [[unlikely]]
            Logger::Warn("Semaphore reduction is unimplemented!");

        u64 address{registers.semaphore->address};
        u64 payload{registers.semaphore->payload};
        switch (registers.launchDma->semaphoreType) {
            case Registers::LaunchDma::SemaphoreType::ReleaseOneWordSemaphore:
                channelCtx.asCtx->gmmu.Write(address, payload);
                Logger::Debug("address: 0x{:X} payload: {}", address, payload);
                break;
            case Registers::LaunchDma::SemaphoreType::ReleaseFourWordSemaphore: {
                // Write timestamp first to ensure correct ordering
                u64 timestamp{GetGpuTimeTicks()};
                channelCtx.asCtx->gmmu.Write(address + 8, timestamp);
                channelCtx.asCtx->gmmu.Write(address, payload);
                Logger::Debug("address: 0x{:X} payload: {} timestamp: {}", address, payload, timestamp);
                break;
            }
            default:
                break;
        }
    }

    void MaxwellDma::CallMethodBatchNonInc(u32 method, span<u32> arguments) {
        for (u32 argument : arguments)
            HandleMethod(method, argument);
    }
}
