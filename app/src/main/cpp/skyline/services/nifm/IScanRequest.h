// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::nifm {
    /**
     * @brief IScanRequest is used by applications to scan for networks
     * @url https://switchbrew.org/wiki/Network_Interface_services#IScanRequest
     */
    class IScanRequest : public BaseService {
      public:
        IScanRequest(const DeviceState &state, ServiceManager &manager);
    };
}
