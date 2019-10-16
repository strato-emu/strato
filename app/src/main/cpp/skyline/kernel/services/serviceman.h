#pragma once

#include <nce.h>
#include <kernel/types/KSession.h>
#include "base_service.h"

namespace skyline::kernel::service {
    /**
     * @brief The ServiceManager class manages passing IPC requests to the right Service and running event loops of Services
     */
    class ServiceManager {
      private:
        const DeviceState &state; //!< The state of the device
        std::vector<std::shared_ptr<BaseService>> serviceVec; //!< A vector with all of the services

        /**
         * @param serviceType The type of service requested
         * @return A shared pointer to an instance of the service
         */
        std::shared_ptr<BaseService> GetService(const Service serviceType);

      public:
        /**
         * @param state The state of the device
         */
        ServiceManager(const DeviceState &state);

        /**
         * @brief Creates a new service and returns it's handle
         * @param serviceType The type of the service
         * @return Handle to KService object of the service
         */
        handle_t NewSession(const Service serviceType);

        /**
         * @brief Creates a new service and writes it's handle or virtual handle (If it's a domain request) to IpcResponse
         * @param serviceType The type of the service
         * @param session The session object of the command
         * @param response The response object to write the handle or virtual handle to
         */
        std::shared_ptr<BaseService> NewService(const Service serviceType, type::KSession &session, ipc::IpcResponse &response);

        /**
         * @brief Closes an existing session to a service
         * @param service The handle of the KService object
         */
        void CloseSession(const handle_t handle);

        /**
         * @brief This is a function where the Services get to run their core event loops
         */
        void Loop();

        /**
         * @brief Handles a Synchronous IPC Request
         * @param handle The handle of the object
         */
        void SyncRequestHandler(const handle_t handle);

        /**
         * @brief Duplicates a session
         * @param handle The handle of the object
         */
        void CloneSession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
