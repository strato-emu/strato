// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief IApplicationProxyService is used to open an application proxy (https://switchbrew.org/wiki/Applet_Manager_services#appletOE)
     */
    class IApplicationProxyService : public BaseService {
      public:
        IApplicationProxyService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns #IApplicationProxy (https://switchbrew.org/wiki/Applet_Manager_services#OpenApplicationProxy)
         */
        Result OpenApplicationProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IApplicationProxyService, OpenApplicationProxy)
        )
    };
}
