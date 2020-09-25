// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <cxxabi.h>
#include "base_service.h"

namespace skyline::service {
    const std::string &BaseService::GetName() {
        if (name.empty()) {
            auto mangledName = typeid(*this).name();

            int status{};
            size_t length{};
            std::unique_ptr<char, decltype(&std::free)> demangled{abi::__cxa_demangle(mangledName, nullptr, &length, &status), std::free};

            name = (status == 0) ? std::string(demangled.get() + std::char_traits<char>::length("skyline::service::")) : mangledName;
        }
        return name;
    }

    Result service::BaseService::HandleRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::pair<std::function<Result(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>, std::string_view> function;
        try {
            function = GetServiceFunction(request.payload->value);
            state.logger->Debug("Service: {} @ {}", function.second, GetName());
        } catch (const std::out_of_range &) {
            state.logger->Warn("Cannot find function in service '{0}': 0x{1:X} ({1})", GetName(), static_cast<u32>(request.payload->value));
            return {};
        }
        try {
            return function.first(session, request, response);
        } catch (const std::exception &e) {
            throw exception("{} (Service: {} @ {})", e.what(), function.second, GetName());
        }
    }
}
