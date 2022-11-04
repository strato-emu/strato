// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::capsrv {
    /**
     * @brief Used by applications to save screenshots
     * @url https://switchbrew.org/wiki/Applet_Manager_services#caps:su
     */
    class IScreenShotApplicationService : public BaseService {
      public:
        IScreenShotApplicationService(const DeviceState &state, ServiceManager &manager);
    };
}