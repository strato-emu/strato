// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "vic.h"

namespace skyline::soc::host1x {
    VicClass::VicClass(std::function<void()> opDoneCallback)
        : opDoneCallback(std::move(opDoneCallback)) {}

    void VicClass::CallMethod(u32 method, u32 argument) {
        Logger::Warn("Unknown VIC class method called: 0x{:X} argument: 0x{:X}", method, argument);
    }
}
