// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>
#include "IAccountServiceForApplication.h"

namespace skyline::service::account {
    /**
    * @brief IManagerForApplication provides functions for reading Nintendo Online user information
    * @url https://switchbrew.org/wiki/Account_services#IManagerForApplication
    */
    class IManagerForApplication : public BaseService {
      private:
        std::shared_ptr<std::vector<UserId>> openedUsers;

      public:
        IManagerForApplication(const DeviceState &state, ServiceManager &manager, std::vector<UserId> &openedUsers);

        /**
        * @brief This checks if the given user has access to online services
        */
        Result CheckAvailability(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the user ID of the current user
         */
        Result GetAccountId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result StoreOpenContext(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IManagerForApplication, CheckAvailability),
            SFUNC(0x1, IManagerForApplication, GetAccountId),
            SFUNC(0xA0, IManagerForApplication, StoreOpenContext)
        )
    };
}