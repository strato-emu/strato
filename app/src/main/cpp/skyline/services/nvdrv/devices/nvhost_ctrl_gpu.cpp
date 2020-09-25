// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "nvhost_ctrl_gpu.h"

namespace skyline::service::nvdrv::device {
    NvHostCtrlGpu::NvHostCtrlGpu(const DeviceState &state) : errorNotifierEvent(std::make_shared<type::KEvent>(state)), unknownEvent(std::make_shared<type::KEvent>(state)), NvDevice(state) {}

    NvStatus NvHostCtrlGpu::ZCullGetCtxSize(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        buffer.as<u32>() = 0x1;
        return NvStatus::Success;
    }

    NvStatus NvHostCtrlGpu::ZCullGetInfo(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct ZCullInfo {
            u32 widthAlignPixels{0x20};
            u32 heightAlignPixels{0x20};
            u32 pixelSquaresByAliquots{0x400};
            u32 aliquotTotal{0x800};
            u32 regionByteMultiplier{0x20};
            u32 regionHeaderSize{0x20};
            u32 subregionHeaderSize{0xC0};
            u32 subregionWidthAlignPixels{0x20};
            u32 subregionHeightAlignPixels{0x40};
            u32 subregionCount{0x10};
        } zCullInfo;

        buffer.as<ZCullInfo>() = zCullInfo;
        return NvStatus::Success;
    }

    NvStatus NvHostCtrlGpu::GetCharacteristics(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct GpuCharacteristics {
            u32 arch{0x120};                             // NVGPU_GPU_ARCH_GM200
            u32 impl{0xB};                               // 0xB (NVGPU_GPU_IMPL_GM20B) or 0xE (NVGPU_GPU_IMPL_GM20B_B)
            u32 rev{0xA1};
            u32 numGpc{0x1};
            u64 l2CacheSize{0x40000};
            u64 onBoardVideoMemorySize{};                // UMA
            u32 numTpcPerGpc{0x2};
            u32 busType{0x20};                           // NVGPU_GPU_BUS_TYPE_AXI
            u32 bigPageSize{0x20000};
            u32 compressionPageSize{0x20000};
            u32 pdeCoverageBitCount{0x1B};
            u32 availableBigPageSizes{0x30000};
            u32 gpcMask{0x1};
            u32 smArchSmVersion{0x503};                  // Maxwell Generation 5.0.3
            u32 smArchSpaVersion{0x503};                 // Maxwell Generation 5.0.3
            u32 smArchWarpCount{0x80};
            u32 gpuVaBitCount{0x28};
            u32 reserved{};
            u64 flags{0x55};                             // HAS_SYNCPOINTS | SUPPORT_SPARSE_ALLOCS | SUPPORT_CYCLE_STATS | SUPPORT_CYCLE_STATS_SNAPSHOT
            u32 twodClass{0x902D};                       // FERMI_TWOD_A
            u32 threedClass{0xB197};                     // MAXWELL_B
            u32 computeClass{0xB1C0};                    // MAXWELL_COMPUTE_B
            u32 gpfifoClass{0xB06F};                     // MAXWELL_CHANNEL_GPFIFO_A
            u32 inlineToMemoryClass{0xA140};             // KEPLER_INLINE_TO_MEMORY_B
            u32 dmaCopyClass{0xA140};                    // MAXWELL_DMA_COPY_A
            u32 maxFbpsCount{0x1};                       // 0x1
            u32 fbpEnMask{};                             // Disabled
            u32 maxLtcPerFbp{0x2};
            u32 maxLtsPerLtc{0x1};
            u32 maxTexPerTpc{};                          // Not Supported
            u32 maxGpcCount{0x1};
            u32 ropL2EnMask0{0x21D70};                   // fuse_status_opt_rop_l2_fbp_r
            u32 ropL2EnMask1{};
            u64 chipName{util::MakeMagic<u64>("gm20b")};
            u64 grCompbitStoreBaseHw{};                  // Not Supported
        };

        struct Data {
            u64 gpuCharacteristicsBufSize;         // InOut
            u64 gpuCharacteristicsBufAddr;         // In
            GpuCharacteristics gpuCharacteristics; // Out
        } &data = buffer.as<Data>();

        if (data.gpuCharacteristicsBufSize < sizeof(GpuCharacteristics))
            return NvStatus::InvalidSize;

        data.gpuCharacteristics = GpuCharacteristics{};
        data.gpuCharacteristicsBufSize = sizeof(GpuCharacteristics);

        return NvStatus::Success;
    }

    NvStatus NvHostCtrlGpu::GetTpcMasks(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct Data {
            u32 maskBufSize; // In
            u32 reserved[3]; // In
            u64 maskBuf;     // Out
        } &data = buffer.as<Data>();

        if (data.maskBufSize)
            data.maskBuf = 0x3;

        return NvStatus::Success;
    }

    NvStatus NvHostCtrlGpu::GetActiveSlotMask(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct Data {
            u32 slot{0x07}; // Out
            u32 mask{0x01}; // Out
        } data;
        buffer.as<Data>() = data;
        return NvStatus::Success;
    }

    std::shared_ptr<type::KEvent> NvHostCtrlGpu::QueryEvent(u32 eventId) {
        switch (eventId) {
            case 1:
                return errorNotifierEvent;
            case 2:
                return unknownEvent;
            default:
                return nullptr;
        }
    }
}
