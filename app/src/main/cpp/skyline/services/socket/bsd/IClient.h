// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::socket {
    /**
     * @brief IClient or bsd:u is used by applications create network sockets (https://switchbrew.org/wiki/Sockets_services#bsd:u.2C_bsd:s)
     */
    class IClient : public BaseService {
      public:
        IClient(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This initializes a socket client with the given parameters (https://switchbrew.org/wiki/Sockets_services#Initialize)
         */
        void RegisterClient(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This starts the monitoring of the socket
         */
        void StartMonitoring(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}