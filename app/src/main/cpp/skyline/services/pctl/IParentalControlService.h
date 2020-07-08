// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::pctl {
    /**
     * @brief IParentalControlService is used to access parental control configuration (https://switchbrew.org/wiki/Parental_Control_services#IParentalControlService)
     */
    class IParentalControlService : public BaseService {
      public:
        IParentalControlService(const DeviceState &state, ServiceManager &manager);
    };
}
