// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::prepo {
    /**
     * @brief IPrepoService or prepo:u is used by applications to store statistics (https://switchbrew.org/wiki/BCAT_services#prepo:a.2C_prepo:a2.2C_prepo:m.2C_prepo:u.2C_prepo:s)
     */
    class IPrepoService : public BaseService {
      public:
        IPrepoService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This saves a play report for the given user
         */
        Result SaveReportWithUser(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
