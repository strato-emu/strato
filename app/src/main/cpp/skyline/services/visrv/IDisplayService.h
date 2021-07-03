// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>

namespace skyline::service::hosbinder {
    class IHOSBinderDriver;
}

namespace skyline::service::visrv {
    /**
     * @brief This is the base class for all IDisplayService variants with common functions
     */
    class IDisplayService : public BaseService {
      protected:
        std::shared_ptr<hosbinder::IHOSBinderDriver> hosbinder; //!< The IHOSBinder relayed via this display class

      public:
        IDisplayService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Creates a stray layer using a display's ID and returns a layer ID and the corresponding buffer ID
         */
        Result CreateStrayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Destroys a stray layer by its ID
         */
        Result DestroyStrayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
