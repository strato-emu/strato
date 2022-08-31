// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "services/nvdrv/devices/nvdevice.h"

namespace skyline::service::nvdrv::device::nvhost {
    /**
     * @brief nvhost::CtrlGpu (/dev/nvhost-ctrl-gpu) is used for context independent operations on the underlying GPU
     * @url https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-ctrl-gpu
     */
    class CtrlGpu : public NvDevice {
      private:
        std::shared_ptr<type::KEvent> errorNotifierEvent;
        std::shared_ptr<type::KEvent> unknownEvent;

      public:
        /**
         * @brief Holds hardware characteristics about a GPU, initialised to the GM20B values
         */
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
            u32 _res_{};
            u64 flags{0x55};                             // HAS_SYNCPOINTS | SUPPORT_SPARSE_ALLOCS | SUPPORT_CYCLE_STATS | SUPPORT_CYCLE_STATS_SNAPSHOT
            u32 twodClass{0x902D};                       // FERMI_TWOD_A
            u32 threedClass{0xB197};                     // MAXWELL_B
            u32 computeClass{0xB1C0};                    // MAXWELL_COMPUTE_B
            u32 gpfifoClass{0xB06F};                     // MAXWELL_CHANNEL_GPFIFO_A
            u32 inlineToMemoryClass{0xA140};             // KEPLER_INLINE_TO_MEMORY_B
            u32 dmaCopyClass{0xB0B5};                    // MAXWELL_DMA_COPY_A
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

        /**
         * @brief Contains the Maxwell ZCULL capabilities and configuration
         */
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
        };

        using ZbcColorValue = std::array<u32, 4>;

        CtrlGpu(const DeviceState &state, Driver &driver, Core &core, const SessionContext &ctx);

        /**
         * @brief Returns the zcull context size
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_ZCULL_GET_CTX_SIZE
         */
        PosixResult ZCullGetCtxSize(Out<u32> size);

        /**
         * @brief Returns information about the GPU ZCULL parameters
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_ZCULL_GET_INFO
         */
        PosixResult ZCullGetInfo(Out<ZCullInfo> info);

        /**
         * @brief Sets the zero bandwidth clear parameters
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_ZBC_SET_TABLE
         */
        PosixResult ZbcSetTable(In<ZbcColorValue> colorDs, In<ZbcColorValue> colorL2, In<u32> depth, In<u32> format, In<u32> type);

        /**
         * @brief Returns a struct with certain GPU characteristics
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_GET_CHARACTERISTICS
         */
        PosixResult GetCharacteristics(InOut<u64> size, In<u64> userAddress, Out<GpuCharacteristics> characteristics);

        /**
         * @brief Ioctl3 variant of GetTpcMasks
         */
        PosixResult GetCharacteristics3(span<u8> inlineBuffer, InOut<u64> size, In<u64> userAddress, Out<GpuCharacteristics> characteristics);

        /**
         * @brief Returns the TPC mask value for each GPC
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_GET_TPC_MASKS
         */
        PosixResult GetTpcMasks(In<u32> bufSize, Out<u32> mask);

        /**
         * @brief Ioctl3 variant of GetTpcMasks
         */
        PosixResult GetTpcMasks3(span<u8> inlineBuffer, In<u32> bufSize, Out<u32> mask);

        /**
         * @brief Returns the mask value for a ZBC slot
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_ZBC_GET_ACTIVE_SLOT_MASK
         */
        PosixResult GetActiveSlotMask(Out<u32> slot, Out<u32> mask);

        /**
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_GET_GPU_TIME
         */
        PosixResult GetGpuTime(Out<u64> time);

        std::shared_ptr<type::KEvent> QueryEvent(u32 eventId) override;

        PosixResult Ioctl(IoctlDescriptor cmd, span<u8> buffer) override;

        PosixResult Ioctl3(IoctlDescriptor cmd, span<u8> buffer, span<u8> inlineBuffer) override;
    };
}
