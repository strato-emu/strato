// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nvdevice.h"

namespace skyline::service::nvdrv::device {
    const std::string &NvDevice::GetName() {
        if (name.empty()) {
            auto mangledName = typeid(*this).name();

            int status{};
            size_t length{};
            std::unique_ptr<char, decltype(&std::free)> demangled{abi::__cxa_demangle(mangledName, nullptr, &length, &status), std::free};

            name = (status == 0) ? std::string(demangled.get() + std::char_traits<char>::length("skyline::service::nvdrv::device::")) : mangledName;
        }
        return name;
    }

    NvStatus NvDevice::HandleIoctl(u32 cmd, IoctlType type, std::span<u8> buffer, std::span<u8> inlineBuffer) {
        std::pair<std::function<NvStatus(IoctlType, std::span<u8>, std::span<u8>)>, std::string_view> function;
        try {
            function = GetIoctlFunction(cmd);
            state.logger->Debug("IOCTL @ {}: {}", GetName(), function.second);
        } catch (std::out_of_range &) {
            state.logger->Warn("Cannot find IOCTL for device '{}': 0x{:X}", GetName(), cmd);
            return NvStatus::NotImplemented;
        }
        try {
            return function.first(type, buffer, inlineBuffer);
        } catch (std::exception &e) {
            throw exception("{} (Device: {})", e.what(), GetName());
        }
        exit(0);
    }
}
