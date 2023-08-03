// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::bcat {
    /**
     * @brief IBcatService is used to interact with BCAT (Background Content Asymmetric synchronized delivery and Transmission)
     * @url https://switchbrew.org/wiki/BCAT_services#IBcatService
     */
    class IBcatService : public BaseService {
      public:
        IBcatService(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/BCAT_services#RequestSyncDeliveryCache
         */

        Result RequestSyncDeliveryCache(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x2774, IBcatService, RequestSyncDeliveryCache)
        )
    };
}
