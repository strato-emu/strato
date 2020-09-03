// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "base_proxy.h"

namespace skyline::service::am {
    /**
     * @brief IApplicationProxy returns handles to various services (https://switchbrew.org/wiki/Applet_Manager_services#IApplicationProxy)
     */
    class IApplicationProxy : public BaseProxy {
      public:
        IApplicationProxy(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns #IApplicationFunctions (https://switchbrew.org/wiki/Applet_Manager_services#IApplicationFunctions)
         */
        Result GetApplicationFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
