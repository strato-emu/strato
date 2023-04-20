// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>
#include <netdb.h>

namespace skyline::service::socket {
    enum class NetDbError : i32 {
        Internal = -1,
        Success = 0,
        HostNotFound = 1,
        TryAgain = 2,
        NoRecovery = 3,
        NoData = 4,
    };

    /**
     * @url https://switchbrew.org/wiki/Sockets_services#sfdnsres
     */
    class IResolver : public BaseService {
      public:
        IResolver(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/Sockets_services#GetAddrInfoRequest
         */
        Result GetAddrInfoRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetHostByNameRequestWithOptions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetAddrInfoRequestWithOptions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetNameInfoRequestWithOptions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        std::pair<u32, i32> GetAddrInfoRequestImpl(ipc::IpcRequest &request);

        std::vector<u8> SerializeAddrInfo(const addrinfo* addrinfo, i32 result_code, std::string_view host);

        SERVICE_DECL(
            SFUNC(0x6, IResolver, GetAddrInfoRequest),
            SFUNC(0xA, IResolver, GetHostByNameRequestWithOptions),
            SFUNC(0xC, IResolver, GetAddrInfoRequestWithOptions),
            SFUNC(0xD, IResolver, GetNameInfoRequestWithOptions)
        )
    };
}
