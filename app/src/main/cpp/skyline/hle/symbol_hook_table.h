// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "symbol_hooks.h"

namespace skyline::hle {
    struct HookTableEntry {
        std::string_view name; //!< The name of the symbol
        HookType hook; //!< The hook that the symbol should include

        HookTableEntry(std::string_view name, HookType hook) : name{name}, hook{std::move(hook)} {}
    };

    static std::array<HookTableEntry, 0> HookedSymbols{};
}
