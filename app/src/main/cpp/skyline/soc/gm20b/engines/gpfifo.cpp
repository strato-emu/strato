// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <soc.h>
#include <soc/gm20b/channel.h>
#include "gpfifo.h"

namespace skyline::soc::gm20b::engine {
    GPFIFO::GPFIFO(host1x::SyncpointSet &syncpoints, ChannelContext &channelCtx) : syncpoints(syncpoints), channelCtx(channelCtx) {}

    void GPFIFO::CallMethod(u32 method, u32 argument) {
        Logger::Debug("Called method in GPFIFO: 0x{:X} args: 0x{:X}", method, argument);

        registers.raw[method] = argument;

        switch (method) {
            ENGINE_STRUCT_CASE(syncpoint, action, {
                if (action.operation == Registers::Syncpoint::Operation::Incr) {
                    Logger::Debug("Increment syncpoint: {}", +action.index);
                    channelCtx.executor.Execute();
                    syncpoints.at(action.index).Increment();
                } else if (action.operation == Registers::Syncpoint::Operation::Wait) {
                    Logger::Debug("Wait syncpoint: {}, thresh: {}", +action.index, registers.syncpoint->payload);

                    // Wait forever for another channel to increment
                    syncpoints.at(action.index).Wait(registers.syncpoint->payload, std::chrono::steady_clock::duration::max());
                }
            })
        }
    };
}
