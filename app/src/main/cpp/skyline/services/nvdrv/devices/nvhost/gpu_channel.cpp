// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <soc.h>
#include <services/nvdrv/devices/deserialisation/deserialisation.h>
#include "gpu_channel.h"

namespace skyline::service::nvdrv::device::nvhost {
    GpuChannel::GpuChannel(const DeviceState &state, Core &core, const SessionContext &ctx) :
        NvDevice(state, core, ctx),
        smExceptionBreakpointIntReportEvent(std::make_shared<type::KEvent>(state, false)),
        smExceptionBreakpointPauseReportEvent(std::make_shared<type::KEvent>(state, false)),
        errorNotifierEvent(std::make_shared<type::KEvent>(state, false)) {
        channelSyncpoint = core.syncpointManager.AllocateSyncpoint(false);
    }

    PosixResult GpuChannel::SetNvmapFd(In<core::NvMap::Handle::Id> id) {
        state.logger->Debug("id: {}", id);
        return PosixResult::Success;
    }

    PosixResult GpuChannel::SetTimeout(In<u32> timeout) {
        state.logger->Debug("timeout: {}", timeout);
        return PosixResult::Success;
    }

    PosixResult GpuChannel::SubmitGpfifo(In<u64> userAddress, In<u32> numEntries, InOut<SubmitGpfifoFlags> flags, InOut<Fence> fence, span<soc::gm20b::GpEntry> gpEntries) {
        state.logger->Debug("userAddress: 0x{:X}, numEntries: {},"
                            "flags ( fenceWait: {}, fenceIncrement: {}, hwFormat: {}, suppressWfi: {}, incrementWithValue: {}),"
                            "fence ( id: {}, threshold: {} )",
                            userAddress, numEntries,
                            +flags.fenceWait, +flags.fenceIncrement, +flags.hwFormat, +flags.suppressWfi, +flags.incrementWithValue,
                            fence.id, fence.threshold);

        if (numEntries > gpEntries.size())
            throw exception("GpEntry size mismatch!");

        if (flags.fenceWait) {
            if (flags.incrementWithValue)
                return PosixResult::InvalidArgument;

            if (core.syncpointManager.IsFenceSignalled(fence))
                throw exception("Waiting on a fence through SubmitGpfifo is unimplemented");
        }

        state.soc->gm20b.gpfifo.Push(gpEntries.subspan(0, numEntries));

        fence.id = channelSyncpoint;

        u32 increment{(flags.fenceIncrement ? 2 : 0) + (flags.incrementWithValue ? fence.threshold : 0)};
        fence.threshold = core.syncpointManager.IncrementSyncpointMaxExt(channelSyncpoint, increment);

        if (flags.fenceIncrement)
            throw exception("Incrementing a fence through SubmitGpfifo is unimplemented");

        flags.raw = 0;

        return PosixResult::Success;
    }

    PosixResult GpuChannel::SubmitGpfifo2(span<u8> inlineBuffer, In<u64> userAddress, In<u32> numEntries, InOut<GpuChannel::SubmitGpfifoFlags> flags, InOut<Fence> fence) {
        return SubmitGpfifo(userAddress, numEntries, flags, fence, inlineBuffer.cast<soc::gm20b::GpEntry>());
    }

    PosixResult GpuChannel::AllocObjCtx(In<u32> classId, In<u32> flags, Out<u64> objId) {
        state.logger->Debug("classId: 0x{:X}, flags: 0x{:X}", classId, flags);
        return PosixResult::Success;
    }

    PosixResult GpuChannel::ZcullBind(In<u64> gpuVa, In<u32> mode) {
        state.logger->Debug("gpuVa: 0x{:X}, mode: {}", gpuVa, mode);
        return PosixResult::Success;
    }

    PosixResult GpuChannel::SetErrorNotifier(In<u64> offset, In<u64> size, In<u32> mem) {
        state.logger->Debug("offset: 0x{:X}, size: 0x{:X}, mem: 0x{:X}", offset, size, mem);
        return PosixResult::Success;
    }

    PosixResult GpuChannel::SetPriority(In<u32> priority) {
        state.logger->Debug("priority: {}", priority);
        return PosixResult::Success;
    }

    PosixResult GpuChannel::AllocGpfifoEx2(In<u32> numEntries, In<u32> numJobs, In<u32> flags, Out<Fence> fence) {
        state.logger->Debug("numEntries: {}, numJobs: {}, flags: 0x{:X}", numEntries, numJobs, flags);
        state.soc->gm20b.gpfifo.Initialize(numEntries);

        fence = core.syncpointManager.GetSyncpointFence(channelSyncpoint);

        return PosixResult::Success;
    }

    PosixResult GpuChannel::SetTimeslice(In<u32> timeslice) {
        state.logger->Debug("timeslice: {}", timeslice);
        return PosixResult::Success;
    }

    PosixResult GpuChannel::SetUserData(In<u64> userData) {
        state.logger->Debug("userData: 0x{:X}", userData);
        channelUserData = userData;
        return PosixResult::Success;
    }

    PosixResult GpuChannel::GetUserData(Out<u64> userData) {
        userData = channelUserData;
        return PosixResult::Success;
    }

    std::shared_ptr<type::KEvent> GpuChannel::QueryEvent(u32 eventId) {
        switch (eventId) {
            case 1:
                return smExceptionBreakpointIntReportEvent;
            case 2:
                return smExceptionBreakpointPauseReportEvent;
            case 3:
                return errorNotifierEvent;
            default:
                return nullptr;
        }
    }

#include <services/nvdrv/devices/deserialisation/macro_def.inc>
    static constexpr u32 GpuChannelUserMagic{0x47};
    static constexpr u32 GpuChannelMagic{0x48};

    VARIABLE_IOCTL_HANDLER_FUNC(GpuChannel, ({
        IOCTL_CASE_ARGS(IN,    SIZE(0x4),  MAGIC(GpuChannelMagic),     FUNC(0x1),
                        SetNvmapFd,       ARGS(In<core::NvMap::Handle::Id>))
        IOCTL_CASE_ARGS(IN,    SIZE(0x4),  MAGIC(GpuChannelMagic),     FUNC(0x3),
                        SetTimeout,       ARGS(In<u32>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x10), MAGIC(GpuChannelMagic),     FUNC(0x9),
                        AllocObjCtx,      ARGS(In<u32>, In<u32>, Out<u64>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x10), MAGIC(GpuChannelMagic),     FUNC(0xB),
                        ZcullBind,        ARGS(In<u64>, In<u32>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x18), MAGIC(GpuChannelMagic),     FUNC(0xC),
                        SetErrorNotifier, ARGS(In<u64>, In<u64>, In<u32>))
        IOCTL_CASE_ARGS(IN,    SIZE(0x4),  MAGIC(GpuChannelMagic),     FUNC(0xD),
                        SetPriority,      ARGS(In<u32>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x20), MAGIC(GpuChannelMagic),     FUNC(0x1A),
                        AllocGpfifoEx2,   ARGS(In<u32>, In<u32>, In<u32>, Out<Fence>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x4),  MAGIC(GpuChannelMagic),     FUNC(0x1D),
                        SetTimeslice,     ARGS(In<u32>))
        IOCTL_CASE_ARGS(IN,    SIZE(0x8),  MAGIC(GpuChannelUserMagic), FUNC(0x14),
                        SetUserData,      ARGS(In<u64>))
        IOCTL_CASE_ARGS(OUT,   SIZE(0x8),  MAGIC(GpuChannelUserMagic), FUNC(0x15),
                        GetUserData,      ARGS(Out<u64>))
    }), ({
        VARIABLE_IOCTL_CASE_ARGS(INOUT, MAGIC(GpuChannelMagic), FUNC(0x8),
                                 SubmitGpfifo, ARGS(In<u64>, In<u32>, InOut<SubmitGpfifoFlags>, InOut<Fence>, AutoSizeSpan<soc::gm20b::GpEntry>))
    }))

    INLINE_IOCTL_HANDLER_FUNC(Ioctl2, GpuChannel, ({
        INLINE_IOCTL_CASE_ARGS(INOUT, SIZE(0x18), MAGIC(GpuChannelMagic), FUNC(0x1B),
                               SubmitGpfifo2, ARGS(In<u64>, In<u32>, InOut<SubmitGpfifoFlags>, InOut<Fence>))
    }))
#include <services/nvdrv/devices/deserialisation/macro_undef.inc>
}
