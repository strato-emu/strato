// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "IRootService.h"

namespace skyline::service::visrv {
    /**
     * @url https://switchbrew.org/wiki/Display_services#vi:u
     */
    class IApplicationRootService : public IRootService {
      public:
        IApplicationRootService(const DeviceState &state, ServiceManager &manager) : IRootService(state, manager, PrivilegeLevel::Application) {}

      SERVICE_DECL(
          SFUNC_BASE(0x0, IApplicationRootService, IRootService, GetDisplayService)
      )
    };
}
