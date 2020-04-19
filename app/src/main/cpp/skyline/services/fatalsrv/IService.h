// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::fatalsrv {
    /**
     * @brief IService or fatal:u is used by applications to throw errors (https://switchbrew.org/wiki/Fatal_services#fatal:u)
     */
    class IService : public BaseService {
      public:
        IService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This throws an exception that causes emulation to quit
         */
        void ThrowFatal(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
