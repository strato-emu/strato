// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::aocsrv {
    /**
     * @brief IAddOnContentManager or aoc:u is used by applications to access add-on content information (https://switchbrew.org/wiki/NS_Services#aoc:u)
     */
    class IAddOnContentManager : public BaseService {
      public:
        IAddOnContentManager(const DeviceState &state, ServiceManager &manager);
    };
}
