// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief This contains common various functions
     * @url https://switchbrew.org/wiki/Applet_Manager_services#IAppletCommonFunctions
     */
    class IAppletCommonFunctions : public BaseService {
      public:
        IAppletCommonFunctions(const DeviceState &state, ServiceManager &manager);
    };
}
