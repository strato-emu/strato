// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::nim {
    /**
     * @brief IShopServiceAsync is used by applications to directly communicate with the Nintendo eShop
     * @url https://switchbrew.org/wiki/NIM_services#IShopServiceAsync
     */
    class IShopServiceAsync : public BaseService {
      public:
        IShopServiceAsync(const DeviceState &state, ServiceManager &manager);
    };
}
