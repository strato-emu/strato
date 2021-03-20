// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/ipc.h>

#define SERVICE_STRINGIFY(string) #string
#define SFUNC(id, Class, Function) std::pair<u32, std::pair<Result(Class::*)(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &), const char*>>{id, {&Class::Function, SERVICE_STRINGIFY(Class::Function)}}
#define SFUNC_BASE(id, Class, BaseClass, Function) std::pair<u32, std::pair<Result(Class::*)(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &), const char*>>{id, {&Class::CallBaseFunction<BaseClass, decltype(&BaseClass::Function), &BaseClass::Function>, SERVICE_STRINGIFY(Class::Function)}}
#define SERVICE_DECL_AUTO(name, value) decltype(value) name = value
#define SERVICE_DECL(...)                                                                                      \
private:                                                                                                       \
template<typename BaseClass, typename BaseFunctionType, BaseFunctionType BaseFunction>                         \
Result CallBaseFunction(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {       \
    return (static_cast<BaseClass *>(this)->*BaseFunction)(session, request, response);                        \
}                                                                                                              \
SERVICE_DECL_AUTO(functions, frz::make_unordered_map({__VA_ARGS__}));                                          \
protected:                                                                                                     \
ServiceFunctionDescriptor GetServiceFunction(u32 id) override {                                                \
    auto& function{functions.at(id)};                                                                          \
    return ServiceFunctionDescriptor{                                                                          \
        reinterpret_cast<DerivedService*>(this),                                                               \
        reinterpret_cast<decltype(ServiceFunctionDescriptor::function)>(function.first),                       \
        function.second                                                                                        \
    };                                                                                                         \
}
#define SRVREG(class, ...) std::make_shared<class>(state, manager, ##__VA_ARGS__)

namespace skyline::kernel::type {
    class KSession;
}

namespace skyline::service {
    using namespace kernel;
    using ServiceName = u64; //!< Service names are a maximum of 8 bytes so we use a u64 to store them

    class ServiceManager;

    /**
     * @brief The base class for the HOS service interfaces hosted by sysmodules
     */
    class BaseService {
      private:
        std::string name; //!< The name of the service, it's only assigned after GetName is called and shouldn't be used directly

      protected:
        const DeviceState &state;
        ServiceManager &manager;

        class DerivedService; //!< A placeholder derived class which is used for class function semantics

        /**
         * @brief A per-function descriptor for HLE service functions
         */
        struct ServiceFunctionDescriptor {
            DerivedService *clazz; //!< A pointer to the class that this was derived from, it's used as the 'this' pointer for the function
            Result (DerivedService::*function)(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &); //!< A function pointer to a HLE implementation of the service function
            const char *name; //!< A pointer to a static string in the format "Class::Function" for the specific service class/function

            constexpr Result operator()(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
                return (clazz->*function)(session, request, response);
            }
        };

      public:
        BaseService(const DeviceState &state, ServiceManager &manager) : state(state), manager(manager) {}

        /**
         * @note To be able to extract the name of the underlying class and ensure correct destruction order
         */
        virtual ~BaseService() = default;

        virtual ServiceFunctionDescriptor GetServiceFunction(u32 id) {
            throw std::out_of_range("GetServiceFunction not implemented");
        }

        /**
         * @return A string with the name of the service class
         * @note The lifetime of the returned string is tied to that of the class
         */
        const std::string &GetName();

        /**
         * @brief Handles an IPC Request to a service
         */
        Result HandleRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
