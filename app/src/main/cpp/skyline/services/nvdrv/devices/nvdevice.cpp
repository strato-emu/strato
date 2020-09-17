// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nvdevice.h"

namespace skyline::service::nvdrv::device {
    NvDevice::NvDevice(const DeviceState &state, std::unordered_map <u32, std::function<void(IoctlData &)>> vTable) : state(state), vTable(vTable) {}

    std::string NvDevice::GetName() {
        int status{};
        size_t length{};
        auto mangledName{typeid(*this).name()};

        std::unique_ptr<char, decltype(&std::free)> demangled{ abi::__cxa_demangle(mangledName, nullptr, &length, &status), std::free};

        return (status == 0) ? std::string(demangled.get() + std::char_traits<char>::length("skyline::service::nvdrv::device")) : mangledName;
    }

    void NvDevice::HandleIoctl(u32 cmd, IoctlData &input) {
        std::function<void(IoctlData &)> function;
        try {
            function = vTable.at(cmd);
        } catch (std::out_of_range &) {
            state.logger->Warn("Cannot find IOCTL for device '{}': 0x{:X}", GetName(), cmd);
            input.status = NvStatus::NotImplemented;
            return;
        }
        try {
            function(input);
        } catch (std::exception &e) {
            throw exception("{} (Device: {})", e.what(), GetName());
        }
    }
}
