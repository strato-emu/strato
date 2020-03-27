// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::sm {
    /**
     * @brief IUserInterface or sm: is responsible for providing handles to services (https://switchbrew.org/wiki/Services_API)
     */
    class IUserInterface : public BaseService {
      public:
        IUserInterface(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This initializes the sm: service. It doesn't actually do anything. (https://switchbrew.org/wiki/Services_API#Initialize)
         */
        void Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a handle to a service with it's name passed in as an argument (https://switchbrew.org/wiki/Services_API#GetService)
         */
        void GetService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
