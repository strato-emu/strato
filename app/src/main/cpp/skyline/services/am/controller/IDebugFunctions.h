// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief This has functions that are used for debugging purposes (https://switchbrew.org/wiki/Applet_Manager_services#IDebugFunctions)
     */
    class IDebugFunctions : public BaseService {
      public:
        IDebugFunctions(const DeviceState &state, ServiceManager &manager);
    };
}
