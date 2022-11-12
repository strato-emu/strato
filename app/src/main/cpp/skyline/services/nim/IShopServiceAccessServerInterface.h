// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::nim {
    /**
     * @brief IShopServiceAccessServerInterface or nim:eca is used by applications to open a channel to communicate with the Nintendo eShop
     * @url https://switchbrew.org/wiki/NIM_services#nim:eca
     */
    class IShopServiceAccessServerInterface : public BaseService {
      public:
        IShopServiceAccessServerInterface(const DeviceState &state, ServiceManager &manager);

        Result CreateServerInterface(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result IsLargeResourceAvailable(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IShopServiceAccessServerInterface, CreateServerInterface),
            SFUNC(0x4, IShopServiceAccessServerInterface, IsLargeResourceAvailable)
        )
    };
}
