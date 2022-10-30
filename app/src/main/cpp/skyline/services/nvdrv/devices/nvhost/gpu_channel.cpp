// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <soc.h>
#include <services/nvdrv/devices/deserialisation/deserialisation.h>
#include "gpu_channel.h"

namespace skyline::service::nvdrv::device::nvhost {
    GpuChannel::GpuChannel(const DeviceState &state, Driver &driver, Core &core, const SessionContext &ctx)
        : NvDevice(state, driver, core, ctx),
          smExceptionBreakpointIntReportEvent(std::make_shared<type::KEvent>(state, false)),
          smExceptionBreakpointPauseReportEvent(std::make_shared<type::KEvent>(state, false)),
          errorNotifierEvent(std::make_shared<type::KEvent>(state, false)) {
        channelSyncpoint = core.syncpointManager.AllocateSyncpoint(false);
    }

    static constexpr size_t SyncpointWaitCmdLen{4};
    static void AddSyncpointWaitCmd(span<u32> mem, Fence fence) {
        size_t offset{};

        // gpfifo.regs.syncpoint.payload = fence.threshold
        mem[offset++] = 0x2001001C;
        mem[offset++] = fence.threshold;

        /*
         gpfifo.regs.syncpoint = {
             .index = fence.id
             .operation = SyncpointOperation::Wait
             .waitSwitch = SyncpointWaitSwitch::En
         }
         Then wait is triggered
        */
        mem[offset++] = 0x2001001D;
        mem[offset++] = (fence.id << 8) | 0x10;
    }

    static constexpr size_t SyncpointIncrCmdLen{8};
    static void AddSyncpointIncrCmd(span<u32> mem, Fence fence, bool wfi) {
        size_t offset{};

        if (wfi) {
            // gpfifo.regs.wfi.scope = WfiScope::CurrentScgType
            // Then WFI is triggered
            mem[offset++] = 0x2001001E;
            mem[offset++] = 0;
        }


        // gpfifo.regs.syncpoint.payload = 0
        mem[offset++] = 0x2001001C;
        mem[offset++] = 0;

        /*
         gpfifo.regs.syncpoint = {
             .index = fence.id
             .operation = SyncpointOperation::Incr
         }
         Then increment is triggered
        */
        mem[offset++] = 0x2001001D;
        mem[offset++] = (fence.id << 8) | 0x1;

        // Repeat twice, likely due to HW bugs
        mem[offset++] = 0x2001001D;
        mem[offset++] = (fence.id << 8) | 0x1;

        if (!wfi) {
            mem[offset++] = 0;
            mem[offset++] = 0;
        }
    }

    PosixResult GpuChannel::SetNvmapFd(In<FileDescriptor> fd) {
        Logger::Debug("fd: {}", fd);
        return PosixResult::Success;
    }

    PosixResult GpuChannel::SetTimeout(In<u32> timeout) {
        Logger::Debug("timeout: {}", timeout);
        return PosixResult::Success;
    }

    PosixResult GpuChannel::SubmitGpfifo(In<u64> userAddress, In<u32> numEntries,
                                         InOut<SubmitGpfifoFlags> flags,
                                         InOut<Fence> fence,
                                         span<soc::gm20b::GpEntry> gpEntries) {
        Logger::Debug("userAddress: 0x{:X}, numEntries: {},"
                            "flags ( fenceWait: {}, fenceIncrement: {}, hwFormat: {}, suppressWfi: {}, incrementWithValue: {}),"
                            "fence ( id: {}, threshold: {} )",
                            userAddress, numEntries,
                            +flags.fenceWait, +flags.fenceIncrement, +flags.hwFormat, +flags.suppressWfi, +flags.incrementWithValue,
                            fence.id, fence.threshold);

        if (numEntries > gpEntries.size())
            throw exception("GpEntry size mismatch!");

        std::scoped_lock lock(channelMutex);

        if (flags.fenceWait) {
            if (flags.incrementWithValue)
                return PosixResult::InvalidArgument;

            if (!core.syncpointManager.IsFenceSignalled(fence)) {
                // Wraparound
                if (pushBufferMemoryOffset + SyncpointWaitCmdLen >= pushBufferMemory.size())
                    pushBufferMemoryOffset = 0;

                AddSyncpointWaitCmd(span(pushBufferMemory).subspan(pushBufferMemoryOffset, SyncpointWaitCmdLen), fence);
                channelCtx->gpfifo.Push(soc::gm20b::GpEntry(pushBufferAddr + pushBufferMemoryOffset * sizeof(u32), SyncpointWaitCmdLen));

                // Increment offset
                pushBufferMemoryOffset += SyncpointWaitCmdLen;
            }
        }

        fence.id = channelSyncpoint;

        u32 increment{(flags.fenceIncrement ? 2 : 0) + (flags.incrementWithValue ? fence.threshold : 0)};
        fence.threshold = core.syncpointManager.IncrementSyncpointMaxExt(channelSyncpoint, increment);

        channelCtx->gpfifo.Push(gpEntries.subspan(0, numEntries));

        if (flags.fenceIncrement) {
            // Wraparound
            if (pushBufferMemoryOffset + SyncpointIncrCmdLen >= pushBufferMemory.size())
                pushBufferMemoryOffset = 0;

            AddSyncpointIncrCmd(span(pushBufferMemory).subspan(pushBufferMemoryOffset, SyncpointIncrCmdLen), fence, !flags.suppressWfi);
            channelCtx->gpfifo.Push(soc::gm20b::GpEntry(pushBufferAddr + pushBufferMemoryOffset * sizeof(u32), SyncpointIncrCmdLen));

            // Increment offset
            pushBufferMemoryOffset += SyncpointIncrCmdLen;
        }

        flags.raw = 0;

        return PosixResult::Success;
    }

    PosixResult GpuChannel::SubmitGpfifo2(span<u8> inlineBuffer, In<u64> userAddress, In<u32> numEntries, InOut<GpuChannel::SubmitGpfifoFlags> flags, InOut<Fence> fence) {
        return SubmitGpfifo(userAddress, numEntries, flags, fence, inlineBuffer.cast<soc::gm20b::GpEntry>());
    }

    PosixResult GpuChannel::AllocObjCtx(In<u32> classId, In<u32> flags, Out<u64> objId) {
        Logger::Debug("classId: 0x{:X}, flags: 0x{:X}", classId, flags);
        return PosixResult::Success;
    }

    PosixResult GpuChannel::ZcullBind(In<u64> gpuVa, In<u32> mode) {
        Logger::Debug("gpuVa: 0x{:X}, mode: {}", gpuVa, mode);
        return PosixResult::Success;
    }

    PosixResult GpuChannel::SetErrorNotifier(In<u64> offset, In<u64> size, In<u32> mem) {
        Logger::Debug("offset: 0x{:X}, size: 0x{:X}, mem: 0x{:X}", offset, size, mem);
        return PosixResult::Success;
    }

    PosixResult GpuChannel::SetPriority(In<u32> priority) {
        Logger::Debug("priority: {}", priority);
        return PosixResult::Success;
    }

    PosixResult GpuChannel::AllocGpfifoEx2(In<u32> numEntries, In<u32> numJobs, In<u32> flags, Out<Fence> fence) {
        Logger::Debug("numEntries: {}, numJobs: {}, flags: 0x{:X}", numEntries, numJobs, flags);

        std::scoped_lock lock(channelMutex);
        if (!asCtx || !asAllocator) {
            Logger::Warn("Trying to allocate a channel without a bound address space");
            return PosixResult::InvalidArgument;
        }

        if (channelCtx) {
            Logger::Warn("Trying to allocate a channel twice!");
            return PosixResult::FileExists;
        }

        channelCtx = std::make_unique<soc::gm20b::ChannelContext>(state, asCtx, numEntries);

        fence = core.syncpointManager.GetSyncpointFence(channelSyncpoint);

        // Allocate space for one wait and incr for each entry, though we're not likely to hit this in practice
        size_t pushBufferWords{numEntries * SyncpointIncrCmdLen + numEntries * SyncpointWaitCmdLen};
        size_t pushBufferSize{pushBufferWords * sizeof(u32)};

        pushBufferMemory.resize(pushBufferWords);

        // Allocate pages in the GPU AS
        pushBufferAddr = static_cast<u64>(asAllocator->Allocate((static_cast<u32>(pushBufferSize) >> AsGpu::VM::PageSizeBits) + 1)) << AsGpu::VM::PageSizeBits;
        if (!pushBufferAddr)
            throw exception("Failed to allocate channel pushbuffer!");

        // Map onto the GPU
        asCtx->gmmu.Map(pushBufferAddr, reinterpret_cast<u8 *>(pushBufferMemory.data()), pushBufferSize);

        return PosixResult::Success;
    }

    PosixResult GpuChannel::SetTimeslice(In<u32> timeslice) {
        Logger::Debug("timeslice: {}", timeslice);
        return PosixResult::Success;
    }

    PosixResult GpuChannel::SetUserData(In<u64> userData) {
        Logger::Debug("userData: 0x{:X}", userData);
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
                        SetNvmapFd,       ARGS(In<FileDescriptor>))
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
