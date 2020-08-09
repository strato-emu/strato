// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::ssl {
    /**
     * @brief ISslService or ssl is used by applications to manage SSL connections (https://switchbrew.org/wiki/SSL_services#ssl)
     */
    class ISslService : public BaseService {
      public:
        ISslService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This creates an SSL context (https://switchbrew.org/wiki/SSL_services#CreateContext)
         */
        void CreateContext(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This sets the SSL interface version (https://switchbrew.org/wiki/SSL_services#SetInterfaceVersion)
         */
        void SetInterfaceVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
