// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <cxxabi.h>
#include "symbol_hooks.h"
#include "symbol_hook_table.h"

namespace skyline::hle {
    std::string Demangle(const std::string_view mangledName) {
        int status{};
        size_t length{};
        std::unique_ptr<char, decltype(&std::free)> demangled{abi::__cxa_demangle(mangledName.data(), nullptr, &length, &status), std::free};
        return std::string{status == 0 ? std::string_view{demangled.get()} : mangledName};
    }

    HookedSymbol::HookedSymbol(std::string pName, HookType hook) : name{std::move(pName)}, prettyName{Demangle(name)}, hook{std::move(hook)} {}

    HookedSymbolEntry::HookedSymbolEntry(std::string name, const HookType &hook, Elf64_Addr *offset) : HookedSymbol{std::move(name), hook}, offset{offset} {}

    std::vector<HookedSymbolEntry> GetExecutableSymbols(span<Elf64_Sym> dynsym, span<char> dynstr) {
        std::vector<HookedSymbolEntry> executableSymbols{};

        if constexpr (HookedSymbols.empty())
            return executableSymbols;

        for (auto &symbol : dynsym) {
            if (symbol.st_name == 0 || symbol.st_value == 0)
                continue;

            if (ELF64_ST_TYPE(symbol.st_info) != STT_FUNC || ELF64_ST_BIND(symbol.st_info) != STB_GLOBAL || symbol.st_shndx == SHN_UNDEF)
                continue;

            std::string_view symbolName{dynstr.data() + symbol.st_name};

            auto item{std::find_if(HookedSymbols.begin(), HookedSymbols.end(), [&symbolName](const auto &item) {
                return item.name == symbolName;
            })};
            if (item != HookedSymbols.end()) {
                executableSymbols.emplace_back(std::string{symbolName}, item->hook, &symbol.st_value);
                continue;
            }

            #ifdef PRINT_HOOK_ALL
            if (symbolName == "memcpy" || symbolName == "memcmp" || symbolName == "memset" || symbolName == "strcmp" ||  symbolName == "strlen")
                // If symbol is from libc (such as memcpy, strcmp, strlen, etc), we don't need to hook it
                continue;

                executableSymbols.emplace_back(std::string{symbolName}, EntryExitHook{
                    .entry = [](const DeviceState &, const HookedSymbol &symbol) {
                        Logger::Debug("Entering \"{}\" ({})", symbol.prettyName, symbol.name);
                    },
                    .exit = [](const DeviceState &, const HookedSymbol &symbol) {
                        Logger::Debug("Exiting \"{}\"", symbol.prettyName);
                    },
                }, &symbol.st_value);
            #endif
        }

        return executableSymbols;
    }
}
