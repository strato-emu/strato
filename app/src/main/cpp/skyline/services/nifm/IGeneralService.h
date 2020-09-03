// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::nifm {
    /**
     * @brief IGeneralService is used by applications to control the network connection (https://switchbrew.org/wiki/Network_Interface_services#IGeneralService)
     */
    class IGeneralService : public BaseService {
      public:
        IGeneralService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This creates an IRequest instance that can be used to bring up the network (https://switchbrew.org/wiki/Network_Interface_services#CreateRequest)
         */
        Result CreateRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
