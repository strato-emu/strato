// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::socket {
    /**
     * @url https://switchbrew.org/wiki/Sockets_services#nsd:u.2C_nsd:a
     */
    class IManager : public BaseService {
      public:
        IManager(const DeviceState &state, ServiceManager &manager);
    };
}
