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

        /**
         * @brief Selects the socket
         */
        Result Select(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Polls the socket for events
         */
        Result Poll(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Receives message from the socket
         */
        Result Recv(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Receives message from the socket
         */
        Result RecvFrom(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Transmits one or more messages to the socket
         */
        Result Send(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Transmits one or more messages to the socket
         */
        Result SendTo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Accepts a connection on the socket
         */
        Result Accept(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Assigns the local protocol address to a socket
         */
        Result Bind(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Initiates a connection on a socket
         */
        Result Connect(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Places a socket in a state in which it is listening for an incoming connection
         */
        Result Listen(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Manipulates the options associated with a socket
         */
        Result SetSockOpt(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Shutdowns the socket
         */
        Result Shutdown(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Shutdowns all sockets
         */
        Result ShutdownAllSockets(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Writes to the socket
         */
        Result Write(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Reads from the socket
         */
        Result Read(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Closes the socket
         */
        Result Close(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IClient, RegisterClient),
            SFUNC(0x1, IClient, StartMonitoring),
            SFUNC(0x5, IClient, Select),
            SFUNC(0x6, IClient, Poll),
            SFUNC(0x8, IClient, Recv),
            SFUNC(0x9, IClient, RecvFrom),
            SFUNC(0xA, IClient, Send),
            SFUNC(0xB, IClient, SendTo),
            SFUNC(0xC, IClient, Accept),
            SFUNC(0xD, IClient, Bind),
            SFUNC(0xE, IClient, Connect),
            SFUNC(0x12, IClient, Listen),
            SFUNC(0x15, IClient, SetSockOpt),
            SFUNC(0x16, IClient, Shutdown),
            SFUNC(0x17, IClient, ShutdownAllSockets),
            SFUNC(0x18, IClient, Write),
            SFUNC(0x19, IClient, Read),
            SFUNC(0x1A, IClient, Close)
        )
    };
}
