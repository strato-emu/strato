// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KSession.h>
#include "base_service.h"

namespace skyline::service {
    /**
     * @brief Holds global service state for service data that persists across sessions
     */
    struct GlobalServiceState;

    /**
     * @brief The ServiceManager class manages passing IPC requests to the right Service and running event loops of Services
     */
    class ServiceManager {
      private:
        const DeviceState &state;
        std::unordered_map<ServiceName, std::shared_ptr<BaseService>> serviceMap; //!< A mapping from a Service to the underlying object
        std::mutex mutex; //!< Synchronizes concurrent access to services to prevent crashes

        /**
         * @brief Creates an instance of the service if it doesn't already exist, otherwise returns an existing instance
         * @param name The name of the service to create
         * @return A shared pointer to an instance of the service
         */
        std::shared_ptr<BaseService> CreateService(ServiceName name);

      public:
        std::shared_ptr<BaseService> smUserInterface; //!< Used by applications to open connections to services
        std::shared_ptr<GlobalServiceState> globalServiceState;

        ServiceManager(const DeviceState &state);

        /**
         * @brief Creates a new service using its type enum and writes its handle or virtual handle (If it's a domain request) to IpcResponse
         * @param name The service's name
         * @param session The session object of the command
         * @param response The response object to write the handle or virtual handle to
         */
        std::shared_ptr<BaseService> NewService(ServiceName name, type::KSession &session, ipc::IpcResponse &response);

        /**
         * @brief Registers a service object in the manager and writes its handle or virtual handle (If it's a domain request) to IpcResponse
         * @param serviceObject An instance of the service
         * @param session The session object of the command
         * @param response The response object to write the handle or virtual handle to
         * @param submodule If the registered service is a submodule or not
         * @param name The name of the service to register if it's not a submodule - it will be added to the service map
         */
        void RegisterService(std::shared_ptr<BaseService> serviceObject, type::KSession &session, ipc::IpcResponse &response);

        template<typename ServiceType>
        void RegisterService(std::shared_ptr<ServiceType> serviceObject, type::KSession &session, ipc::IpcResponse &response) {
            RegisterService(std::static_pointer_cast<BaseService>(serviceObject), session, response);
        }

        /**
         * @param serviceType The type of the service
         * @tparam The class of the service
         * @return A shared pointer to an instance of the service
         * @note This only works for services created with `NewService` as sub-interfaces used with `RegisterService` can have multiple instances
         */
        template<typename Type>
        std::shared_ptr<Type> GetService(ServiceName name) {
            return std::static_pointer_cast<Type>(serviceMap.at(name));
        }

        template<typename Type>
        constexpr std::shared_ptr<Type> GetService(std::string_view name) {
            return GetService<Type>(util::MakeMagic<ServiceName>(name));
        }

        /**
         * @brief Closes an existing session to a service
         * @param service The handle of the KService object
         */
        void CloseSession(KHandle handle);

        /**
         * @brief Handles a Synchronous IPC Request
         * @param handle The handle of the object
         */
        void SyncRequestHandler(KHandle handle);
    };
}
