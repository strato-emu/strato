// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::hle {
    struct HookedSymbol;

    using HookFunction = std::function<void(const DeviceState &, const HookedSymbol &)>;

    using OverrideHook = HookFunction; //!< A function that overrides the execution of a hooked function

    struct EntryExitHook {
        HookFunction entry; //!< The hook to be called when the function is entered
        HookFunction exit; //!< The hook to be called when the function is exited
    };

    using HookType = std::variant<OverrideHook, EntryExitHook>;

    struct HookedSymbol {
        std::string name; //!< The name of the symbol
        std::string prettyName; //!< The demangled name of the symbol
        HookType hook; //!< The hook that the symbol should include

        HookedSymbol(std::string name, HookType hook);
    };
}
