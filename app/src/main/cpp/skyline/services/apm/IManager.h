// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::apm {
    /**
     * @brief IManager is mostly only used to open an ISession (https://switchbrew.org/wiki/PPC_services#apm)
     */
    class IManager : public BaseService {
      public:
        IManager(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns an handle to ISession
         */
        void OpenSession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
