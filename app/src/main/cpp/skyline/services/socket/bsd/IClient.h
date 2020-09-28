// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::socket {
    /**
     * @brief IClient or bsd:u is used by applications create network sockets
     * @url https://switchbrew.org/wiki/Sockets_services#bsd:u.2C_bsd:s
     */
    class IClient : public BaseService {
      public:
        IClient(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Initializes a socket client with the given parameters
         * @url https://switchbrew.org/wiki/Sockets_services#Initialize
         */
        Result RegisterClient(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Starts the monitoring of the socket
         */
        Result StartMonitoring(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IClient, RegisterClient),
            SFUNC(0x1, IClient, StartMonitoring)
        )
    };
}
