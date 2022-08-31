// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/utils.h>
#include <services/nvdrv/devices/deserialisation/deserialisation.h>
#include "ctrl_gpu.h"

namespace skyline::service::nvdrv::device::nvhost {
    CtrlGpu::CtrlGpu(const DeviceState &state, Driver &driver, Core &core, const SessionContext &ctx)
        : NvDevice(state, driver, core, ctx),
          errorNotifierEvent(std::make_shared<type::KEvent>(state, false)),
          unknownEvent(std::make_shared<type::KEvent>(state, false)) {}

    PosixResult CtrlGpu::ZCullGetCtxSize(Out<u32> size) {
        size = 0x1;
        return PosixResult::Success;
    }

    PosixResult CtrlGpu::ZCullGetInfo(Out<ZCullInfo> info) {
        info = {};
        return PosixResult::Success;
    }

    PosixResult CtrlGpu::ZbcSetTable(In<ZbcColorValue> colorDs, In<ZbcColorValue> colorL2, In<u32> depth, In<u32> format, In<u32> type) {
        return PosixResult::Success;
    }

    PosixResult CtrlGpu::GetCharacteristics(InOut<u64> size, In<u64> userAddress, Out<GpuCharacteristics> characteristics) {
        characteristics = {};
        size = sizeof(GpuCharacteristics);
        return PosixResult::Success;
    }

    PosixResult CtrlGpu::GetCharacteristics3(span<u8> inlineBuffer, InOut<u64> size, In<u64> userAddress, Out<GpuCharacteristics> characteristics) {
        inlineBuffer.as<GpuCharacteristics>() = {};
        return GetCharacteristics(size, userAddress, characteristics);
    }

    PosixResult CtrlGpu::GetTpcMasks(In<u32> bufSize, Out<u32> mask) {
        if (bufSize)
            mask = 0x3;

        return PosixResult::Success;
    }

    PosixResult CtrlGpu::GetTpcMasks3(span<u8> inlineBuffer, In<u32> bufSize, Out<u32> mask) {
        if (bufSize)
            mask = inlineBuffer.as<u32>() = 0x3;

        return PosixResult::Success;
    }

    PosixResult CtrlGpu::GetActiveSlotMask(Out<u32> slot, Out<u32> mask) {
        slot = 0x7;
        mask = 0x1;
        return PosixResult::Success;
    }

    PosixResult CtrlGpu::GetGpuTime(Out<u64> time) {
        time = static_cast<u64>(util::GetTimeNs());
        return PosixResult::Success;
    }

    std::shared_ptr<type::KEvent> CtrlGpu::QueryEvent(u32 eventId) {
        switch (eventId) {
            case 1:
                return errorNotifierEvent;
            case 2:
                return unknownEvent;
            default:
                return nullptr;
        }
    }

#include <services/nvdrv/devices/deserialisation/macro_def.inc>
    static constexpr u32 CtrlGpuMagic{0x47};

    IOCTL_HANDLER_FUNC(CtrlGpu, ({
        IOCTL_CASE_ARGS(OUT,   SIZE(0x4),  MAGIC(CtrlGpuMagic), FUNC(0x1),
                        ZCullGetCtxSize,    ARGS(Out<u32>))
        IOCTL_CASE_ARGS(OUT,   SIZE(0x28), MAGIC(CtrlGpuMagic), FUNC(0x2),
                        ZCullGetInfo,       ARGS(Out<ZCullInfo>))
        IOCTL_CASE_ARGS(IN,    SIZE(0x2C), MAGIC(CtrlGpuMagic), FUNC(0x3),
                        ZbcSetTable,        ARGS(In<ZbcColorValue>, In<ZbcColorValue>, In<u32>, In<u32>, In<u32>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0xB0), MAGIC(CtrlGpuMagic), FUNC(0x5),
                        GetCharacteristics, ARGS(InOut<u64>, In<u64>, Out<GpuCharacteristics>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x18), MAGIC(CtrlGpuMagic), FUNC(0x6),
                        GetTpcMasks,        ARGS(In<u32>, Pad<u32, 3>, Out<u32>))
        IOCTL_CASE_ARGS(OUT,   SIZE(0x8),  MAGIC(CtrlGpuMagic), FUNC(0x14),
                        GetActiveSlotMask,  ARGS(Out<u32>, Out<u32>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x10), MAGIC(CtrlGpuMagic), FUNC(0x1C),
                        GetGpuTime,         ARGS(Out<u64>, Pad<u64>))
    }))

    INLINE_IOCTL_HANDLER_FUNC(Ioctl3, CtrlGpu, ({
        INLINE_IOCTL_CASE_ARGS(INOUT, SIZE(0xB0), MAGIC(CtrlGpuMagic), FUNC(0x5),
                               GetCharacteristics3, ARGS(InOut<u64>, In<u64>, Out<GpuCharacteristics>))
        INLINE_IOCTL_CASE_ARGS(INOUT, SIZE(0x18), MAGIC(CtrlGpuMagic), FUNC(0x6),
                               GetTpcMasks3,        ARGS(In<u32>, Pad<u32, 3>, Out<u32>))
    }))
#include <services/nvdrv/devices/deserialisation/macro_undef.inc>
}
