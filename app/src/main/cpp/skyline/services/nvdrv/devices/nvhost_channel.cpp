// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <span>
#include <os.h>
#include <gpu/gpfifo.h>
#include <kernel/types/KProcess.h>
#include <services/nvdrv/INvDrvServices.h>
#include "nvhost_channel.h"

namespace skyline::service::nvdrv::device {
    NvHostChannel::NvHostChannel(const DeviceState &state, NvDeviceType type) : smExceptionBreakpointIntReportEvent(std::make_shared<type::KEvent>(state)), smExceptionBreakpointPauseReportEvent(std::make_shared<type::KEvent>(state)), errorNotifierEvent(std::make_shared<type::KEvent>(state)), NvDevice(state, type, {
        {0x4801, NFUNC(NvHostChannel::SetNvmapFd)},
        {0x4803, NFUNC(NvHostChannel::SetSubmitTimeout)},
        {0x4808, NFUNC(NvHostChannel::SubmitGpfifo)},
        {0x4809, NFUNC(NvHostChannel::AllocObjCtx)},
        {0x480B, NFUNC(NvHostChannel::ZcullBind)},
        {0x480C, NFUNC(NvHostChannel::SetErrorNotifier)},
        {0x480D, NFUNC(NvHostChannel::SetPriority)},
        {0x481A, NFUNC(NvHostChannel::AllocGpfifoEx2)},
        {0x4714, NFUNC(NvHostChannel::SetUserData)},
    }) {
        auto &hostSyncpoint = state.os->serviceManager.GetService<nvdrv::INvDrvServices>(Service::nvdrv_INvDrvServices)->hostSyncpoint;

        channelFence.id = hostSyncpoint.AllocateSyncpoint(false);
        channelFence.UpdateValue(hostSyncpoint);
    }

    void NvHostChannel::SetNvmapFd(IoctlData &buffer) {}

    void NvHostChannel::SetSubmitTimeout(IoctlData &buffer) {}

    void NvHostChannel::SubmitGpfifo(IoctlData &buffer) {
        struct Data {
            u64 address;
            u32 numEntries;
            union {
                struct __attribute__((__packed__)) {
                    bool fenceWait : 1;
                    bool fenceIncrement : 1;
                    bool hwFormat : 1;
                    u8 _pad0_ : 1;
                    bool suppressWfi : 1;
                    u8 _pad1_ : 3;
                    bool incrementWithValue : 1;
                };
                u32 raw;
            } flags;
            Fence fence;
        } args = state.process->GetReference<Data>(buffer.output.at(0).address);

        auto &hostSyncpoint = state.os->serviceManager.GetService<nvdrv::INvDrvServices>(Service::nvdrv_INvDrvServices)->hostSyncpoint;

        if (args.flags.fenceWait) {
            if (args.flags.incrementWithValue) {
                buffer.status = NvStatus::BadValue;
                return;
            }

            if (hostSyncpoint.HasSyncpointExpired(args.fence.id, args.fence.value))
                throw exception("Waiting on a fence through SubmitGpfifo is unimplemented");
        }

        state.gpu->gpfifo.Push(std::span(state.process->GetPointer<gpu::gpfifo::GpEntry>(args.address), args.numEntries));

        args.fence.id = channelFence.id;

        u32 increment = (args.flags.fenceIncrement ? 2 : 0) + (args.flags.incrementWithValue ? args.fence.value : 0);
        args.fence.value = hostSyncpoint.IncrementSyncpointMaxExt(args.fence.id, increment);

        if (args.flags.fenceIncrement)
            throw exception("Incrementing a fence through SubmitGpfifo is unimplemented");

        args.flags.raw = 0;
    }

    void NvHostChannel::AllocObjCtx(IoctlData &buffer) {}

    void NvHostChannel::ZcullBind(IoctlData &buffer) {}

    void NvHostChannel::SetErrorNotifier(IoctlData &buffer) {}

    void NvHostChannel::SetPriority(IoctlData &buffer) {
        auto priority = state.process->GetObject<NvChannelPriority>(buffer.input.at(0).address);

        switch (priority) {
            case NvChannelPriority::Low:
                timeslice = 1300;
                break;
            case NvChannelPriority::Medium:
                timeslice = 2600;
                break;
            case NvChannelPriority::High:
                timeslice = 5200;
                break;
        }
    }

    void NvHostChannel::AllocGpfifoEx2(IoctlData &buffer) {
        struct Data {
            u32 numEntries;
            u32 numJobs;
            u32 flags;
            Fence fence;
            u32 reserved[3];
        } args = state.process->GetReference<Data>(buffer.input.at(0).address);

        channelFence.UpdateValue(state.os->serviceManager.GetService<nvdrv::INvDrvServices>(Service::nvdrv_INvDrvServices)->hostSyncpoint);
        args.fence = channelFence;
    }

    void NvHostChannel::SetUserData(IoctlData &buffer) {}

    std::shared_ptr<type::KEvent> NvHostChannel::QueryEvent(u32 eventId) {
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

}
