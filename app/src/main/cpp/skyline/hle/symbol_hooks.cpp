// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <cxxabi.h>
#include "symbol_hooks.h"

namespace skyline::hle {
    std::string Demangle(const std::string_view mangledName) {
        int status{};
        size_t length{};
        std::unique_ptr<char, decltype(&std::free)> demangled{abi::__cxa_demangle(mangledName.data(), nullptr, &length, &status), std::free};
        return std::string{status == 0 ? std::string_view{demangled.get()} : mangledName};
    }

    HookedSymbol::HookedSymbol(std::string pName, HookType hook) : name{std::move(pName)}, prettyName{Demangle(name)}, hook{std::move(hook)} {}
}
