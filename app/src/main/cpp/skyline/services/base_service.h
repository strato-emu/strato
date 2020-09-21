// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <functional>
#include <cxxabi.h>
#include <kernel/ipc.h>
#include <common.h>

#define SFUNC(id, Class, Function) std::pair<u32, std::pair<std::function<Result(Class*, type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>, std::string_view>>{id, {&Class::Function, #Function}}
#define SFUNC_BASE(id, Class, BaseClass, Function) std::pair<u32, std::pair<std::function<Result(Class*, type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>, std::string_view>>{id, {&CallBaseFunction<Class, BaseClass, decltype(&BaseClass::Function), &BaseClass::Function>, #Function}}
#define SERVICE_DECL_AUTO(name, value) decltype(value) name = value
#define SERVICE_DECL(...)                                                                                                                         \
SERVICE_DECL_AUTO(functions, frz::make_unordered_map({__VA_ARGS__}));                                                                             \
std::pair<std::function<Result(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>, std::string_view> GetServiceFunction(u32 id) {          \
    auto& function = functions.at(id);                                                                                                            \
    return std::make_pair(std::bind(function.first, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), function.second); \
}
#define SRVREG(class, ...) std::make_shared<class>(state, manager, ##__VA_ARGS__)

namespace skyline::kernel::type {
    class KSession;
}

namespace skyline::service {
    using namespace kernel;
    using ServiceName = u64; //!< Service names are a maximum of 8 bytes so we use a u64 to reference them

    class ServiceManager;

    /**
     * @brief The base class for all service interfaces hosted by sysmodules
     */
    class BaseService {
      private:
        std::string name; //!< The name of the service

      protected:
        const DeviceState &state; //!< The state of the device
        ServiceManager &manager; //!< A reference to the service manager

        template<typename Class, typename BaseClass, typename BaseFunctionType, BaseFunctionType BaseFunction>
        static constexpr Result CallBaseFunction(Class* clazz, type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
            return (static_cast<BaseClass*>(clazz)->*BaseFunction)(session, request, response);
        }

      public:
        /**
         * @param state The state of the device
         * @param vTable The functions of the service
         */
        BaseService(const DeviceState &state, ServiceManager &manager) : state(state), manager(manager) {}

        /**
         * @note To be able to extract the name of the underlying class and ensure correct destruction order
         */
        virtual ~BaseService() = default;

        virtual std::pair<std::function<Result(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>, std::string_view> GetServiceFunction(u32 id) {
            throw std::out_of_range("GetServiceFunction not implemented");
        }

        /**
         * @return The name of the class
         * @note The lifetime of the returned string is tied to that of the class
         */
        const std::string &GetName();

        /**
         * @brief This handles all IPC commands with type request to a service
         * @param request The corresponding IpcRequest object
         * @param response The corresponding IpcResponse object
         */
        Result HandleRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);;
    };
}
