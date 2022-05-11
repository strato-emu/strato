// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>
#include "IBtmUserCore.h"

namespace skyline::service::btm {
    /**
     * @brief IBtmUser is used to create a BtmUserCore instance
     * @url https://switchbrew.org/wiki/BTM_services#btm:u
     */
    class IBtmUser : public BaseService {
      public:
        IBtmUser(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/BTM_services#GetCore_2
         */
        Result GetCore(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IBtmUser, GetCore)
        )
    };
}
