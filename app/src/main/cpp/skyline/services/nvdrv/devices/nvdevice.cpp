// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <cxxabi.h>
#include <common/trace.h>
#include "nvdevice.h"

namespace skyline::service::nvdrv::device {
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

    NvStatus NvDevice::HandleIoctl(u32 cmd, IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        std::string_view typeString{[type] {
            switch (type) {
                case IoctlType::Ioctl:
                    return "IOCTL";
                case IoctlType::Ioctl2:
                    return "IOCTL2";
                case IoctlType::Ioctl3:
                    return "IOCTL3";
            }
        }()};

        NvDeviceFunctionDescriptor function;
        try {
            function = GetIoctlFunction(cmd);
            state.logger->DebugNoPrefix("{}: {}", typeString, function.name);
        } catch (std::out_of_range &) {
            state.logger->Warn("Cannot find IOCTL for device '{}': 0x{:X}", GetName(), cmd);
            return NvStatus::NotImplemented;
        }
        TRACE_EVENT("service", perfetto::StaticString{function.name});
        try {
            return function(type, buffer, inlineBuffer);
        } catch (const std::exception &e) {
            throw exception("{} ({}: {})", e.what(), typeString, function.name);
        }
    }
}
