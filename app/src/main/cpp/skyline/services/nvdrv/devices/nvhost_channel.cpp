// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "nvhost_channel.h"

namespace skyline::service::nvdrv::device {
    NvHostChannel::NvHostChannel(const DeviceState &state, NvDeviceType type) : NvDevice(state, type, {
        {0x4801, NFUNC(NvHostChannel::SetNvmapFd)},
        {0x4803, NFUNC(NvHostChannel::SetSubmitTimeout)},
        {0x4808, NFUNC(NvHostChannel::SubmitGpFifo)},
        {0x4809, NFUNC(NvHostChannel::AllocObjCtx)},
        {0x480B, NFUNC(NvHostChannel::ZcullBind)},
        {0x480C, NFUNC(NvHostChannel::SetErrorNotifier)},
        {0x480D, NFUNC(NvHostChannel::SetPriority)},
        {0x481A, NFUNC(NvHostChannel::AllocGpfifoEx2)},
        {0x4714, NFUNC(NvHostChannel::SetUserData)},
    }) {}

    void NvHostChannel::SetNvmapFd(IoctlData &buffer) {}

    void NvHostChannel::SetSubmitTimeout(IoctlData &buffer) {}

    void NvHostChannel::SubmitGpFifo(IoctlData &buffer) {}

    void NvHostChannel::AllocObjCtx(IoctlData &buffer) {}

    void NvHostChannel::ZcullBind(IoctlData &buffer) {}

    void NvHostChannel::SetErrorNotifier(IoctlData &buffer) {}

    void NvHostChannel::SetPriority(IoctlData &buffer) {
        auto priority = state.process->GetObject<NvChannelPriority>(buffer.input[0].address);

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

    void NvHostChannel::AllocGpfifoEx2(IoctlData &buffer) {}

    void NvHostChannel::SetUserData(IoctlData &buffer) {}

}
