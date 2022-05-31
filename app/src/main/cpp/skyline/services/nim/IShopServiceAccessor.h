// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::nim {
    /**
     * @brief IShopServiceAccessor is used by applications to communicate with the Nintendo eShop
     * @url https://switchbrew.org/wiki/NIM_services#IShopServiceAccessor
     */
    class IShopServiceAccessor : public BaseService {
      public:
        IShopServiceAccessor(const DeviceState &state, ServiceManager &manager);

        Result CreateAsyncInterface(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IShopServiceAccessor, CreateAsyncInterface)
        )
    };
}
