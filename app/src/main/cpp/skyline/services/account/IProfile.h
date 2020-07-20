// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "IAccountServiceForApplication.h"
#include <services/base_service.h>

namespace skyline::service::account {

    /**
    * @brief IProfile provides functions for reading user profile (https://switchbrew.org/wiki/Account_services#IProfile)
    */
    class IProfile : public BaseService {
      public:
        IProfile(const DeviceState &state, ServiceManager &manager, const UserId &userId);

      private:

        UserId userId;

        /**
         * @brief This returns AccountUserData and AccountProfileBase objects that describe the user's information
         */
        void Get(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
