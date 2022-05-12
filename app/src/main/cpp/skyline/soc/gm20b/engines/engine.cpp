// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "engine.h"

namespace skyline::soc::gm20b::engine {
    u64 GetGpuTimeTicks() {
        constexpr i64 NsToTickNumerator{384};
        constexpr i64 NsToTickDenominator{625};

        i64 nsTime{util::GetTimeNs()};
        i64 timestamp{(nsTime / NsToTickDenominator) * NsToTickNumerator + ((nsTime % NsToTickDenominator) * NsToTickNumerator) / NsToTickDenominator};
        return static_cast<u64>(timestamp);
    }

    MacroEngineBase::MacroEngineBase(MacroState &macroState) : macroState(macroState) {}

    void MacroEngineBase::HandleMacroCall(u32 macroMethodOffset, u32 argument, bool lastCall) {
        // Starting a new macro at index 'macroMethodOffset / 2'
        if (!(macroMethodOffset & 1)) {
            // Flush the current macro as we are switching to another one
            if (macroInvocation.Valid()) {
                macroState.macroInterpreter.Execute(macroState.macroPositions[macroInvocation.index], macroInvocation.arguments, this);
                macroInvocation.Reset();
            }

            // Setup for the new macro index
            macroInvocation.index = (macroMethodOffset / 2) % macroState.macroPositions.size();
        }

        macroInvocation.arguments.emplace_back(argument);

        // Flush macro after all of the data in the method call has been sent
        if (lastCall && macroInvocation.Valid()) {
            macroState.macroInterpreter.Execute(macroState.macroPositions[macroInvocation.index], macroInvocation.arguments, this);
            macroInvocation.Reset();
        }
    };
}
