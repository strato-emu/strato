// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::ssl {
    /**
     * @brief ISslContext is used to manage SSL certificates (https://switchbrew.org/wiki/SSL_services#ISslContext)
     */
    class ISslContext : public BaseService {
      public:
        ISslContext(const DeviceState &state, ServiceManager &manager);
    };
}
