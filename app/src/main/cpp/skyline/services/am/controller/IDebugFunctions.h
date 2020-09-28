// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief This is used for debugging purposes
     * @url https://switchbrew.org/wiki/Applet_Manager_services#IDebugFunctions
     */
    class IDebugFunctions : public BaseService {
      public:
        IDebugFunctions(const DeviceState &state, ServiceManager &manager);
    };
}
