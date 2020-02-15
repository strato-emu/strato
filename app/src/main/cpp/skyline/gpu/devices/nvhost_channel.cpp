#include "nvhost_channel.h"
#include <kernel/types/KProcess.h>

namespace skyline::gpu::device {
    NvHostChannel::NvHostChannel(const DeviceState &state, NvDeviceType type) : NvDevice(state, type, {
        {0x40044801, NFUNC(NvHostChannel::SetNvmapFd)},
        {0xC0104809, NFUNC(NvHostChannel::AllocObjCtx)},
        {0xC010480B, NFUNC(NvHostChannel::ZcullBind)},
        {0xC018480C, NFUNC(NvHostChannel::SetErrorNotifier)},
        {0x4004480D, NFUNC(NvHostChannel::SetPriority)},
        {0xC020481A, NFUNC(NvHostChannel::AllocGpfifoEx2)},
        {0x40084714, NFUNC(NvHostChannel::SetUserData)}
    }) {}

    void NvHostChannel::SetNvmapFd(skyline::gpu::device::IoctlData &buffer) {}

    void NvHostChannel::AllocObjCtx(skyline::gpu::device::IoctlData &buffer) {}

    void NvHostChannel::ZcullBind(IoctlData &buffer) {}

    void NvHostChannel::SetErrorNotifier(skyline::gpu::device::IoctlData &buffer) {}

    void NvHostChannel::SetPriority(skyline::gpu::device::IoctlData &buffer) {
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

    void NvHostChannel::AllocGpfifoEx2(skyline::gpu::device::IoctlData &buffer) {}

    void NvHostChannel::SetUserData(skyline::gpu::device::IoctlData &buffer) {}

}
