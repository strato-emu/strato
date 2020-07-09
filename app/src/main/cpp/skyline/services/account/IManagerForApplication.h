// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::account {
    /**
    * @brief IManagerForApplication provides functions for reading Nintendo Online user information (https://switchbrew.org/wiki/Account_services#IManagerForApplication)
    */
    class IManagerForApplication : public BaseService {
      public:
        IManagerForApplication(const DeviceState &state, ServiceManager &manager);
    };
}