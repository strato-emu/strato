// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::bcat {
    /**
     * @brief IDeliveryCacheStorageService is used to create files instances for BCAT
     * @url https://switchbrew.org/wiki/BCAT_services#IDeliveryCacheStorageService
     */
    class IDeliveryCacheStorageService : public BaseService {
      public:
        IDeliveryCacheStorageService(const DeviceState &state, ServiceManager &manager);
    };
}
