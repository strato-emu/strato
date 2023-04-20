// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>
#include <netinet/in.h>

namespace skyline::service::socket {
    enum class OptionName : u32 {
        ReuseAddr = 0x4,
        Broadcast = 0x20,
        Linger = 0x80,
        SndBuf = 0x1001,
        RcvBuf = 0x1002,
        SndTimeo = 0x1005,
        RcvTimeo = 0x1006,
    };
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

        Result Socket(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

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
         * @brief Retrieves the address of the peer to which a socket is connected
         */
        Result GetPeerName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Retrieves the current local address of the socket
         */
        Result GetSockName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Retrieves socket options
         */
        Result GetSockOpt(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Places a socket in a state in which it is listening for an incoming connection
         */
        Result Listen(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Performs a control operation on an open file descriptor
         * @url https://switchbrew.org/wiki/Sockets_services#Fcntl
         */
        Result Fcntl(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

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

        Result EventFd(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result PushBsdResult(ipc::IpcResponse &response, i32 result, i32 errorCode);

        Result PushBsdResultErrno(ipc::IpcResponse &response, i64 result);

        /**
         * @brief Translates option name to a socket option
         */
        i32 GetOption(OptionName optionName);

        SERVICE_DECL(
            SFUNC(0x0, IClient, RegisterClient),
            SFUNC(0x1, IClient, StartMonitoring),
            SFUNC(0x2, IClient, Socket),
            SFUNC(0x6, IClient, Poll),
            SFUNC(0x8, IClient, Recv),
            SFUNC(0x9, IClient, RecvFrom),
            SFUNC(0xA, IClient, Send),
            SFUNC(0xB, IClient, SendTo),
            SFUNC(0xC, IClient, Accept),
            SFUNC(0xD, IClient, Bind),
            SFUNC(0xE, IClient, Connect),
            SFUNC(0xF, IClient, GetPeerName),
            SFUNC(0x10, IClient, GetSockName),
            SFUNC(0x11, IClient, GetSockOpt),
            SFUNC(0x12, IClient, Listen),
            SFUNC(0x14, IClient, Fcntl),
            SFUNC(0x15, IClient, SetSockOpt),
            SFUNC(0x16, IClient, Shutdown),
            SFUNC(0x17, IClient, ShutdownAllSockets),
            SFUNC(0x18, IClient, Write),
            SFUNC(0x19, IClient, Read),
            SFUNC(0x1A, IClient, Close),
            SFUNC(0x1F, IClient, EventFd)
        )
    };
}
