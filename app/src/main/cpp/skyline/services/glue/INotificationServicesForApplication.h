// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>

namespace skyline::service::glue {
    /**
     * @brief Stub implementation for INotificationServicesForApplication
     * @url https://switchbrew.org/wiki/Glue_services#notif:a
     */
    class INotificationServicesForApplication : public BaseService {
      public:
        INotificationServicesForApplication(const DeviceState &state, ServiceManager &manager);
    };
}
