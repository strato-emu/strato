// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <functional>
#include <cxxabi.h>
#include <kernel/ipc.h>
#include <common.h>

#define SFUNC(function) std::bind(&function, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
#define SRVREG(class) std::make_shared<class>(state, manager)

namespace skyline::kernel::type {
    class KSession;
}

namespace skyline::service {
    using namespace kernel;
    using ServiceName = u64; //!< Service names are a maximum of 8 bytes so we use a u64 to reference them

    class ServiceManager;

    /**
     * @brief The BaseService class is a class for all Services to inherit from
     */
    class BaseService {
      protected:
        const DeviceState &state; //!< The state of the device
        ServiceManager &manager; //!< A reference to the service manager
        std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> vTable; //!< This holds the mapping from a function's CmdId to the actual function

      public:
        /**
         * @param state The state of the device
         * @param vTable The functions of the service
         */
        BaseService(const DeviceState &state, ServiceManager &manager, const std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> &vTable) : state(state), manager(manager), vTable(vTable) {}

        /**
         * @note To be able to extract the name of the underlying class and ensure correct destruction order
         */
        virtual ~BaseService() = default;

        std::string GetName() {
            int status{};
            size_t length{};
            auto mangledName{typeid(*this).name()};

            std::unique_ptr<char, decltype(&std::free)> demangled{ abi::__cxa_demangle(mangledName, nullptr, &length, &status), std::free};

            return (status == 0) ? std::string(demangled.get() + std::char_traits<char>::length("skyline::service::")) : mangledName;
        }

        /**
         * @brief This handles all IPC commands with type request to a service
         * @param request The corresponding IpcRequest object
         * @param response The corresponding IpcResponse object
         */
        void HandleRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
            std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)> function;
            try {
                function = vTable.at(request.payload->value);
            } catch (std::out_of_range &) {
                state.logger->Warn("Cannot find function in service '{0}': 0x{1:X} ({1})", GetName(), static_cast<u32>(request.payload->value));
                return;
            }

            try {
                function(session, request, response);
            } catch (std::exception &e) {
                throw exception("{} (Service: {})", e.what(), GetName());
            }
        };
    };
}
