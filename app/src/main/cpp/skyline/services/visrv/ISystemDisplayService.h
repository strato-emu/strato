// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "IDisplayService.h"

namespace skyline::service::visrv {
    /**
     * @brief This service retrieves information about a display in context of the entire system
     * @url https://switchbrew.org/wiki/Display_services#ISystemDisplayService
     */
    class ISystemDisplayService : public IDisplayService {
      public:
        ISystemDisplayService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Sets the Z index of a layer
         */
        Result SetLayerZ(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

      SERVICE_DECL(
          SFUNC(0x89D, ISystemDisplayService, SetLayerZ),
          SFUNC_BASE(0x908, ISystemDisplayService, IDisplayService, CreateStrayLayer)
      )
    };
}
