// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <cxxabi.h>
#include "nvdevice.h"

namespace skyline::service::nvdrv::device {
    NvDevice::NvDevice(const DeviceState &state, Core &core, const SessionContext &ctx) : state(state), core(core), ctx(ctx) {}

    const std::string &NvDevice::GetName() {
        if (name.empty()) {
            auto mangledName{typeid(*this).name()};

            int status{};
            size_t length{};
            std::unique_ptr<char, decltype(&std::free)> demangled{abi::__cxa_demangle(mangledName, nullptr, &length, &status), std::free};

            name = (status == 0) ? std::string(demangled.get() + std::char_traits<char>::length("skyline::service::nvdrv::device::")) : mangledName;
        }
        return name;
    }
}
