// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::ro {
    /**
     * @brief IRoInterface or ldr:ro is used by applications to dynamically load nros
     * @url https://switchbrew.org/wiki/RO_services#LoadModule
     */
    class IRoInterface : public BaseService {
      public:
        IRoInterface(const DeviceState &state, ServiceManager &manager);
    };
}
