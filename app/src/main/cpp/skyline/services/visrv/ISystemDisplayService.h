#pragma once

#include "IDisplayService.h"

namespace skyline::service::visrv {
    /**
     * @brief This service retrieves information about a display in context of the entire system (https://switchbrew.org/wiki/Display_services#ISystemDisplayService)
     */
    class ISystemDisplayService : public IDisplayService {
      public:
        ISystemDisplayService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Sets the Z index of a layer
         */
        void SetLayerZ(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
