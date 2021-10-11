// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <soc.h>
#include <soc/gm20b/channel.h>
#include "gpfifo.h"

namespace skyline::soc::gm20b::engine {
    GPFIFO::GPFIFO(const DeviceState &state, ChannelContext &channelCtx) : Engine(state), channelCtx(channelCtx) {}

    void GPFIFO::CallMethod(u32 method, u32 argument, bool lastCall) {
        state.logger->Debug("Called method in GPFIFO: 0x{:X} args: 0x{:X}", method, argument);

        registers.raw[method] = argument;

        #define GPFIFO_OFFSET(field) U32_OFFSET(Registers, field)
        #define GPFIFO_STRUCT_OFFSET(field, member) GPFIFO_OFFSET(field) + U32_OFFSET(typeof(Registers::field), member)

        #define GPFIFO_CASE_BASE(fieldName, fieldAccessor, offset, content) case offset: { \
            auto fieldName{util::BitCast<typeof(registers.fieldAccessor)>(argument)};      \
            content                                                                        \
            return;                                                                        \
        }
        #define GPFIFO_CASE(field, content) GPFIFO_CASE_BASE(field, field, GPFIFO_OFFSET(field), content)
        #define GPFIFO_STRUCT_CASE(field, member, content) GPFIFO_CASE_BASE(member, field.member, GPFIFO_STRUCT_OFFSET(field, member), content)

        switch (method) {
            GPFIFO_STRUCT_CASE(syncpoint, action, {
                if (action.operation == Registers::SyncpointOperation::Incr) {
                    state.logger->Debug("Increment syncpoint: {}", +action.index);
                    channelCtx.executor.Execute();
                    state.soc->host1x.syncpoints.at(action.index).Increment();
                } else if (action.operation == Registers::SyncpointOperation::Wait) {
                    state.logger->Debug("Wait syncpoint: {}, thresh: {}", +action.index, registers.syncpoint.payload);

                    // Wait forever for another channel to increment
                    state.soc->host1x.syncpoints.at(action.index).Wait(registers.syncpoint.payload, std::chrono::steady_clock::duration::max());
                }
            })
        }

        #undef GPFIFO_STRUCT_CASE
        #undef GPFIFO_CASE
        #undef GPFIFO_CASE_BASE
        #undef GPFIFO_STRUCT_OFFSET
        #undef GPFIFO_OFFSET
    };
}
