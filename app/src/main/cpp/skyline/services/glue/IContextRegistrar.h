// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>

namespace skyline::service::glue {
    /**
     * @brief Stub implementation for IContextRegistrar
     * @url https://switchbrew.org/wiki/Glue_services#IContextRegistrar
     */
    class IContextRegistrar : public BaseService {
      public:
        IContextRegistrar(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
    };
}
