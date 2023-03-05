// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "engine.h"

namespace skyline::soc::gm20b::engine {
    u64 GetGpuTimeTicks() {
        constexpr i64 NsToTickNumerator{384};
        constexpr i64 NsToTickDenominator{625};

        i64 nsTime{util::GetTimeNs()};
        i64 timestamp{(nsTime / NsToTickDenominator) * NsToTickNumerator + ((nsTime % NsToTickDenominator) * NsToTickNumerator) / NsToTickDenominator};

        // By reporting that less time has passed on the  GPU than has actually passed we can avoid dynamic resolution kicking in
        // TODO: add a setting for this after global settings
        return static_cast<u64>(timestamp / 256);
    }

    MacroEngineBase::MacroEngineBase(MacroState &macroState) : macroState(macroState) {}

    bool MacroEngineBase::HandleMacroCall(u32 macroMethodOffset, GpfifoArgument argument, bool lastCall, const std::function<void(void)> &flushCallback) {
        // Starting a new macro at index 'macroMethodOffset / 2'
        if (!(macroMethodOffset & 1)) {
            // Flush the current macro as we are switching to another one
            if (macroInvocation.Valid()) {
                macroState.Execute(macroInvocation.index, macroInvocation.arguments, this, flushCallback);
                macroInvocation.Reset();
            }

            // Setup for the new macro index
            macroInvocation.index = (macroMethodOffset / 2) % macroState.macroPositions.size();
        }

        macroInvocation.arguments.emplace_back(argument);

        // Flush macro after all of the data in the method call has been sent
        if (lastCall && macroInvocation.Valid()) {
            macroState.Execute(macroInvocation.index, macroInvocation.arguments, this, flushCallback);
            macroInvocation.Reset();
            return false;
        }

        return true;
    };
}
