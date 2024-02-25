// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <linux/elf.h>

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

    struct HookedSymbolEntry : HookedSymbol {
        Elf64_Addr* offset{}; //!< A pointer to the hooked function's offset (st_value) in the ELF's dynsym, this is set by the loader and is used to resolve/update the address of the function

        HookedSymbolEntry(std::string name, const hle::HookType &hook, Elf64_Addr* offset);
    };

    /**
     * @brief Gets the hooked symbols information required for patching from the given executable's dynsym section
     * @param dynsym The dynsym section of the executable
     * @param dynstr The dynstr section of the executable
     * @return A vector of HookedSymbolEntry that contains an entry per hooked symbols
     */
    std::vector<HookedSymbolEntry> GetExecutableSymbols(span<Elf64_Sym> dynsym, span<char> dynstr);
}
