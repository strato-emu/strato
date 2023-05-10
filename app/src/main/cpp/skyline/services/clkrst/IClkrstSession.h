// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::clkrst {

    /**
     * @url https://switchbrew.org/wiki/PCV_services#IClkrstSession
     */
    class IClkrstSession : public BaseService {
      public:
        IClkrstSession(const DeviceState &state, ServiceManager &manager);
    };
}
