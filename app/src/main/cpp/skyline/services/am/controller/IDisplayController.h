// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief This is used to capture the contents of a display
     * @url https://switchbrew.org/wiki/Applet_Manager_services#IDisplayController
     */
    class IDisplayController : public BaseService {
      public:
        IDisplayController(const DeviceState &state, ServiceManager &manager);
    };
}
