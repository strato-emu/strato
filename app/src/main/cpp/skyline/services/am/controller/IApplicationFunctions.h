// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief This has functions that are used to notify an application about it's state (https://switchbrew.org/wiki/Applet_Manager_services#IApplicationFunctions)
     */
    class IApplicationFunctions : public BaseService {
      public:
        IApplicationFunctions(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns if the application is running or not, always returns true (https://switchbrew.org/wiki/Applet_Manager_services#NotifyRunning)
         */
        void NotifyRunning(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
