// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <soc.h>
#include <soc/gm20b/channel.h>
#include "kepler_compute/qmd.h"
#include "kepler_compute.h"

namespace skyline::soc::gm20b::engine {
    KeplerCompute::KeplerCompute(const DeviceState &state, ChannelContext &channelCtx)
        : syncpoints(state.soc->host1x.syncpoints), i2m(channelCtx.asCtx) {}

    __attribute__((always_inline)) void KeplerCompute::CallMethod(u32 method, u32 argument) {
        Logger::Verbose("Called method in Kepler compute: 0x{:X} args: 0x{:X}", method, argument);

        HandleMethod(method, argument);
    }

#define KEPLER_COMPUTE_OFFSET(field) (sizeof(typeof(Registers::field)) - sizeof(std::remove_reference_t<decltype(*Registers::field)>)) / sizeof(u32)
#define KEPLER_COMPUTE_STRUCT_OFFSET(field, member) KEPLER_COMPUTE_OFFSET(field) + U32_OFFSET(std::remove_reference_t<decltype(*Registers::field)>, member)

    void KeplerCompute::HandleMethod(u32 method, u32 argument) {
        registers.raw[method] = argument;

        switch (method) {
            case KEPLER_COMPUTE_STRUCT_OFFSET(i2m, launchDma):
                i2m.LaunchDma(*registers.i2m);
                return;
            case KEPLER_COMPUTE_STRUCT_OFFSET(i2m, loadInlineData):
                i2m.LoadInlineData(*registers.i2m, argument);
                return;
            case KEPLER_COMPUTE_OFFSET(sendSignalingPcasB):
                Logger::Warn("Attempted to execute compute kernel!");
                return;
            case KEPLER_COMPUTE_STRUCT_OFFSET(reportSemaphore, action):
                throw exception("Compute semaphores are unimplemented!");
                return;
            default:
                return;
        }

    }

    void KeplerCompute::CallMethodBatchNonInc(u32 method, span<u32> arguments) {
        switch (method) {
            case KEPLER_COMPUTE_STRUCT_OFFSET(i2m, loadInlineData):
                i2m.LoadInlineData(*registers.i2m, arguments);
                return;
            default:
                break;
        }

        for (u32 argument : arguments)
            HandleMethod(method, argument);
    }

#undef KEPLER_COMPUTE_STRUCT_OFFSET
#undef KEPLER_COMPUTE_OFFSET
}
