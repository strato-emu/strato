// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief This has functions used to capture the contents of a display (https://switchbrew.org/wiki/Applet_Manager_services#IDisplayController)
     */
    class IDisplayController : public BaseService {
      public:
        IDisplayController(const DeviceState &state, ServiceManager &manager);
    };
}
