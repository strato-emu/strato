// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::account {
    /**
    * @brief IManagerForApplication provides functions for reading Nintendo Online user information
    * @url https://switchbrew.org/wiki/Account_services#IManagerForApplication
    */
    class IManagerForApplication : public BaseService {
      public:
        IManagerForApplication(const DeviceState &state, ServiceManager &manager);

        /**
        * @brief This checks if the given user has access to online services
        */
        Result CheckAvailability(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result StoreOpenContext(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IManagerForApplication, CheckAvailability),
            SFUNC(0xA0, IManagerForApplication, StoreOpenContext)
        )
    };
}