// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::fatalsrv {
    /**
     * @brief IService or fatal:u is used by applications to throw errors
     * @url https://switchbrew.org/wiki/Fatal_services#fatal:u
     */
    class IService : public BaseService {
      public:
        IService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Throws an exception that causes emulation to quit
         */
        Result ThrowFatal(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IService, ThrowFatal),
            SFUNC(0x1, IService, ThrowFatal),
            SFUNC(0x2, IService, ThrowFatal)
        )
    };
}
