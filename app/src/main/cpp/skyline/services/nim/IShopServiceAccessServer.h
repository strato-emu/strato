// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>

namespace skyline::service::nim {
    /**
     * @brief IShopServiceAccessServer is used by applications to interface with the Nintendo eShop
     * @url https://switchbrew.org/wiki/NIM_services#IShopServiceAccessServer
     */
    class IShopServiceAccessServer : public BaseService {
      public:
        IShopServiceAccessServer(const DeviceState &state, ServiceManager &manager);

        Result CreateAccessorInterface(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IShopServiceAccessServer, CreateAccessorInterface),
        )
    };
}
