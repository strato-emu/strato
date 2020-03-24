#include "nvhost_ctrl_gpu.h"
#include <kernel/types/KProcess.h>

namespace skyline::service::nvdrv::device {
    NvHostCtrlGpu::NvHostCtrlGpu(const DeviceState &state) : NvDevice(state, NvDeviceType::nvhost_ctrl_gpu, {
        {0x80044701, NFUNC(NvHostCtrlGpu::ZCullGetCtxSize)},
        {0x80284702, NFUNC(NvHostCtrlGpu::ZCullGetInfo)},
        {0xC0184706, NFUNC(NvHostCtrlGpu::GetTpcMasks)},
        {0xC0B04705, NFUNC(NvHostCtrlGpu::GetCharacteristics)},
        {0x80084714, NFUNC(NvHostCtrlGpu::GetActiveSlotMask)}
    }) {}

    void NvHostCtrlGpu::ZCullGetCtxSize(IoctlData &buffer) {
        u32 size = 0x1;
        state.process->WriteMemory(size, buffer.output[0].address);
    }

    void NvHostCtrlGpu::ZCullGetInfo(IoctlData &buffer) {
        struct {
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
        state.process->WriteMemory(zCullInfo, buffer.output[0].address);
    }

    void NvHostCtrlGpu::GetCharacteristics(IoctlData &buffer) {
        struct GpuCharacteristics {
            u32 arch;                       // 0x120 (NVGPU_GPU_ARCH_GM200)
            u32 impl;                       // 0xB (NVGPU_GPU_IMPL_GM20B) or 0xE (NVGPU_GPU_IMPL_GM20B_B)
            u32 rev;                        // 0xA1 (Revision A1)
            u32 numGpc;                     // 0x1
            u64 l2CacheSize;                // 0x40000
            u64 onBoardVideoMemorySize;     // 0x0 (not used)
            u32 numTpcPerGpc;               // 0x2
            u32 busType;                    // 0x20 (NVGPU_GPU_BUS_TYPE_AXI)
            u32 bigPageSize;                // 0x20000
            u32 compressionPageSize;        // 0x20000
            u32 pdeCoverageBitCount;        // 0x1B
            u32 availableBigPageSizes;      // 0x30000
            u32 gpcMask;                    // 0x1
            u32 smArchSmVersion;            // 0x503 (Maxwell Generation 5.0.3)
            u32 smArchSpaVersion;           // 0x503 (Maxwell Generation 5.0.3)
            u32 smArchWarpCount;            // 0x80
            u32 gpuVaBitCount;              // 0x28
            u32 reserved;                   // NULL
            u64 flags;                      // 0x55 (HAS_SYNCPOINTS | SUPPORT_SPARSE_ALLOCS | SUPPORT_CYCLE_STATS | SUPPORT_CYCLE_STATS_SNAPSHOT)
            u32 twodClass;                  // 0x902D (FERMI_TWOD_A)
            u32 threedClass;                // 0xB197 (MAXWELL_B)
            u32 computeClass;               // 0xB1C0 (MAXWELL_COMPUTE_B)
            u32 gpfifoClass;                // 0xB06F (MAXWELL_CHANNEL_GPFIFO_A)
            u32 inlineToMemoryClass;        // 0xA140 (KEPLER_INLINE_TO_MEMORY_B)
            u32 dmaCopyClass;               // 0xB0B5 (MAXWELL_DMA_COPY_A)
            u32 maxFbpsCount;               // 0x1
            u32 fbpEnMask;                  // 0x0 (disabled)
            u32 maxLtcPerFbp;               // 0x2
            u32 maxLtsPerLtc;               // 0x1
            u32 maxTexPerTpc;               // 0x0 (not supported)
            u32 maxGpcCount;                // 0x1
            u32 ropL2EnMask0;               // 0x21D70 (fuse_status_opt_rop_l2_fbp_r)
            u32 ropL2EnMask1;               // 0x0
            u64 chipName;                   // 0x6230326D67 ("gm20b")
            u64 grCompbitStoreBaseHw;       // 0x0 (not supported)
        };
        struct Data {
            u64 gpuCharacteristicsBufSize;         // InOut
            u64 gpuCharacteristicsBufAddr;         // In
            GpuCharacteristics gpuCharacteristics; // Out
        } data = state.process->GetObject<Data>(buffer.input[0].address);
        data.gpuCharacteristics = {
            .arch = 0x120,
            .impl = 0xB,
            .rev = 0xA1,
            .numGpc = 0x1,
            .l2CacheSize = 0x40000,
            .onBoardVideoMemorySize = 0x0,
            .numTpcPerGpc = 0x2,
            .busType = 0x20,
            .bigPageSize = 0x20000,
            .compressionPageSize = 0x20000,
            .pdeCoverageBitCount = 0x1B,
            .availableBigPageSizes = 0x30000,
            .gpcMask = 0x1,
            .smArchSmVersion = 0x503,
            .smArchSpaVersion = 0x503,
            .smArchWarpCount = 0x80,
            .gpuVaBitCount = 0x2,
            .flags = 0x55,
            .twodClass = 0x902D,
            .threedClass = 0xB197,
            .computeClass = 0xB1C0,
            .gpfifoClass = 0xB06F,
            .inlineToMemoryClass = 0xA140,
            .dmaCopyClass = 0xB0B5,
            .maxFbpsCount = 0x1,
            .fbpEnMask = 0x0,
            .maxLtcPerFbp = 0x2,
            .maxLtsPerLtc = 0x1,
            .maxTexPerTpc = 0x0,
            .maxGpcCount = 0x1,
            .ropL2EnMask0 = 0x21D70,
            .ropL2EnMask1 = 0x0,
            .chipName = 0x6230326D67,
            .grCompbitStoreBaseHw = 0x0
        };
        data.gpuCharacteristicsBufSize = 0xA0;
        state.process->WriteMemory(data, buffer.output[0].address);
    }

    void NvHostCtrlGpu::GetTpcMasks(IoctlData &buffer) {
        struct Data {
            u32 maskBufSize; // In
            u32 reserved[3]; // In
            u64 maskBuf;     // Out
        } data = state.process->GetObject<Data>(buffer.input[0].address);
        if (data.maskBufSize)
            data.maskBuf = 0x3;
        state.process->WriteMemory(data, buffer.output[0].address);
    }

    void NvHostCtrlGpu::GetActiveSlotMask(IoctlData &buffer) {
        struct Data {
            u32 slot; // Out
            u32 mask; // Out
        } data = {
            .slot = 0x07,
            .mask = 0x01
        };
        state.process->WriteMemory(data, buffer.output[0].address);
    }
}
