// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::lm {
    /**
     * @brief ILogService is used by applications to open an ILogger for printing log messages (https://switchbrew.org/wiki/Log_services#lm)
     */
    class ILogService : public BaseService {
      public:
        ILogService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This opens an ILogger that can be used by applications to print log messages (https://switchbrew.org/wiki/Log_services#OpenLogger)
         */
        Result OpenLogger(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, ILogService, OpenLogger)
        )
    };
}
