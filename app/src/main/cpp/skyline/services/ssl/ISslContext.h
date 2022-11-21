// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::ssl {
    /**
     * @brief ISslContext is used to manage SSL certificates
     * @url https://switchbrew.org/wiki/SSL_services#ISslContext
     */
    class ISslContext : public BaseService {
      public:
        ISslContext(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/SSL_services#ImportServerPki
         */
        Result ImportServerPki(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x4, ISslContext, ImportServerPki)
        )
    };
}
