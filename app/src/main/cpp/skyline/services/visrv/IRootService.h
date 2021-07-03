// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>

namespace skyline::service::visrv {
    /**
     * @brief All privilege-based variants that a single service can have
     */
    enum class PrivilegeLevel {
        Application, //!< The service used by user applications (Lowest)
        System, //!< The service used by system applications (Higher)
        Manager, //!< The service used by system services internally (Highest)
    };

    /**
     * @brief Manages allocation of VI to display services
     * @url https://switchbrew.org/wiki/Display_services#vi:u
     * @url https://switchbrew.org/wiki/Display_services#vi:s
     * @url https://switchbrew.org/wiki/Display_services#vi:m
     */
    class IRootService : public BaseService {
      private:
        PrivilegeLevel level;

      public:
        IRootService(const DeviceState &state, ServiceManager &manager, PrivilegeLevel level);

        /**
         * @brief Returns an handle to #IApplicationDisplayService
         * @url https://switchbrew.org/wiki/Display_services#GetDisplayService
         */
        Result GetDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
