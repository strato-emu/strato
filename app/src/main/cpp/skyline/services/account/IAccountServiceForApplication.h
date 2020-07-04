// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::account {
    /**
     * @brief IAccountServiceForApplication or acc:u0 provides functions for reading user information (https://switchbrew.org/wiki/Account_services#acc:u0)
     */
    class IAccountServiceForApplication : public BaseService {
      public:
        IAccountServiceForApplication(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This provides information about the running application for account services to use (https://switchbrew.org/wiki/Account_services#InitializeApplicationInfoV0)
         */
        void InitializeApplicationInfoV0(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
