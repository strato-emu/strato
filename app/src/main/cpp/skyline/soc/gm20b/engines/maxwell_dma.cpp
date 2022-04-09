// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <soc.h>
#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include "maxwell_dma.h"

namespace skyline::soc::gm20b::engine {
    MaxwellDma::MaxwellDma(const DeviceState &state, ChannelContext &channelCtx)
        : channelCtx(channelCtx), syncpoints(state.soc->host1x.syncpoints) {}

    __attribute__((always_inline)) void MaxwellDma::CallMethod(u32 method, u32 argument) {
        Logger::Verbose("Called method in Maxwell DMA: 0x{:X} args: 0x{:X}", method, argument);

        HandleMethod(method, argument);
    }

    void MaxwellDma::HandleMethod(u32 method, u32 argument) {
        registers.raw[method] = argument;

        if (method == ENGINE_OFFSET(launchDma))
            LaunchDma();
    }

    void MaxwellDma::LaunchDma() {
        if (*registers.lineLengthIn == 0)
            return; // Nothing to copy

        if (registers.launchDma->multiLineEnable) {
            // 2D/3D copy
            Logger::Warn("2D/3D DMA engine copies are unimplemented");
        } else {
            // 1D buffer copy
            // TODO: implement swizzled 1D copies based on VMM 'kind'
            Logger::Debug("src: 0x{:X} dst: 0x{:X} size: 0x{:X}", u64{*registers.offsetIn}, u64{*registers.offsetOut}, *registers.lineLengthIn);
            channelCtx.asCtx->gmmu.Copy(*registers.offsetOut, *registers.offsetIn, *registers.lineLengthIn);
        }
    }

    void MaxwellDma::CallMethodBatchNonInc(u32 method, span<u32> arguments) {
        for (u32 argument : arguments)
            HandleMethod(method, argument);
    }
}
