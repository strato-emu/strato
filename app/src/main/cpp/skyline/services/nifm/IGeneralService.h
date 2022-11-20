// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::nifm {
    namespace result {
        constexpr Result NoInternetConnection{110, 300};
    }
    /**
     * @brief IGeneralService is used by applications to control the network connection
     * @url https://switchbrew.org/wiki/Network_Interface_services#IGeneralService
     */
    class IGeneralService : public BaseService {
      public:
        IGeneralService(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/Network_Interface_services#CreateScanRequest
         */
        Result CreateScanRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Network_Interface_services#CreateRequest
         */
        Result CreateRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Network_Interface_services#GetCurrentIpAddress
         */
        Result GetCurrentIpAddress(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Network_Interface_services#IsAnyInternetRequestAccepted
         */
        Result IsAnyInternetRequestAccepted(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x1, IGeneralService, CreateScanRequest),
            SFUNC(0x4, IGeneralService, CreateRequest),
            SFUNC(0xC, IGeneralService, GetCurrentIpAddress),
            SFUNC(0x15, IGeneralService, IsAnyInternetRequestAccepted)
        )
    };
}
