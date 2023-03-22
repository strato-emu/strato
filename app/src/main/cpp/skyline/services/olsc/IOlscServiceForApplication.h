// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::olsc {
    /**
     * @url https://switchbrew.org/wiki/OLSC_services#olsc:u
     */
    class IOlscServiceForApplication : public BaseService {
      public:
        IOlscServiceForApplication(const DeviceState &state, ServiceManager &manager);
    };
}
