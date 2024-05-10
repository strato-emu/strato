// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2024 Strato Team and Contributors (https://github.com/strato-emu/)

#pragma once

#include <string>
#include <dynarmic/interface/A32/config.h>

namespace skyline::jit {
    inline std::string to_string(Dynarmic::A32::Exception e) {
        #define CASE(x) case Dynarmic::A32::Exception::x: return #x

        switch (e) {
            CASE(UndefinedInstruction);
            CASE(UnpredictableInstruction);
            CASE(DecodeError);
            CASE(SendEvent);
            CASE(SendEventLocal);
            CASE(WaitForInterrupt);
            CASE(WaitForEvent);
            CASE(Yield);
            CASE(Breakpoint);
            CASE(PreloadData);
            CASE(PreloadDataWithIntentToWrite);
            CASE(PreloadInstruction);
            CASE(NoExecuteFault);
            default:
                return "Unknown";
        }

        #undef CASE
    }
}
