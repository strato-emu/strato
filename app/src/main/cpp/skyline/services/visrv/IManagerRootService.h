// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "IRootService.h"

namespace skyline::service::visrv {
    /**
     * @url https://switchbrew.org/wiki/Display_services#vi:m
     */
    class IManagerRootService : public IRootService {
      public:
        IManagerRootService(const DeviceState &state, ServiceManager &manager) : IRootService(state, manager, PrivilegeLevel::Manager) {}

      SERVICE_DECL(
          SFUNC_BASE(0x2, IManagerRootService, IRootService, GetDisplayService)
      )
    };
}
