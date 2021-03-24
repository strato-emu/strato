// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <soc.h>
#include <kernel/types/KProcess.h>
#include <services/nvdrv/driver.h>
#include "nvhost_channel.h"

namespace skyline::service::nvdrv::device {
    NvHostChannel::NvHostChannel(const DeviceState &state) : smExceptionBreakpointIntReportEvent(std::make_shared<type::KEvent>(state, false)), smExceptionBreakpointPauseReportEvent(std::make_shared<type::KEvent>(state, false)), errorNotifierEvent(std::make_shared<type::KEvent>(state, false)), NvDevice(state) {
        auto driver{nvdrv::driver.lock()};
        auto &hostSyncpoint{driver->hostSyncpoint};

        channelFence.id = hostSyncpoint.AllocateSyncpoint(false);
        channelFence.UpdateValue(hostSyncpoint);
    }

    NvStatus NvHostChannel::SetNvmapFd(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        return NvStatus::Success;
    }

    NvStatus NvHostChannel::SetSubmitTimeout(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        return NvStatus::Success;
    }

    NvStatus NvHostChannel::SubmitGpfifo(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct Data {
            soc::gm20b::GpEntry *entries; // In
            u32 numEntries;                // In
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
            } flags;                       // In
            Fence fence;                   // InOut
        } &data = buffer.as<Data>();

        auto driver{nvdrv::driver.lock()};
        auto &hostSyncpoint{driver->hostSyncpoint};

        if (data.flags.fenceWait) {
            if (data.flags.incrementWithValue)
                return NvStatus::BadValue;

            if (hostSyncpoint.HasSyncpointExpired(data.fence.id, data.fence.value))
                throw exception("Waiting on a fence through SubmitGpfifo is unimplemented");
        }

        state.soc->gm20b.gpfifo.Push([&]() {
            if (type == IoctlType::Ioctl2)
                return inlineBuffer.cast<soc::gm20b::GpEntry>();
            else
                return span(data.entries, data.numEntries);
        }());

        data.fence.id = channelFence.id;

        u32 increment{(data.flags.fenceIncrement ? 2 : 0) + (data.flags.incrementWithValue ? data.fence.value : 0)};
        data.fence.value = hostSyncpoint.IncrementSyncpointMaxExt(data.fence.id, increment);

        if (data.flags.fenceIncrement)
            throw exception("Incrementing a fence through SubmitGpfifo is unimplemented");

        data.flags.raw = 0;

        return NvStatus::Success;
    }

    NvStatus NvHostChannel::AllocObjCtx(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        return NvStatus::Success;
    }

    NvStatus NvHostChannel::ZcullBind(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        return NvStatus::Success;
    }

    NvStatus NvHostChannel::SetErrorNotifier(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        return NvStatus::Success;
    }

    NvStatus NvHostChannel::SetPriority(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        switch (buffer.as<NvChannelPriority>()) {
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

        return NvStatus::Success;
    }

    NvStatus NvHostChannel::AllocGpfifoEx2(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct Data {
            u32 numEntries;  // In
            u32 numJobs;     // In
            u32 flags;       // In
            Fence fence;     // Out
            u32 _res_[3];    // In
        } &data = buffer.as<Data>();

        state.soc->gm20b.gpfifo.Initialize(data.numEntries);

        auto driver{nvdrv::driver.lock()};
        channelFence.UpdateValue(driver->hostSyncpoint);
        data.fence = channelFence;

        return NvStatus::Success;
    }

    NvStatus NvHostChannel::SetTimeslice(IoctlType type, std::span<u8> buffer, std::span<u8> inlineBuffer) {
        return NvStatus::Success;
    }

    NvStatus NvHostChannel::SetUserData(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        return NvStatus::Success;
    }

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
