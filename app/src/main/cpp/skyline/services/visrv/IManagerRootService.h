#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>
#include <gpu.h>

namespace skyline::service::visrv {
    /**
     * @brief This service is used to get an handle to #IApplicationDisplayService (https://switchbrew.org/wiki/Display_services#vi:m)
     */
    class IManagerRootService : public BaseService {
      public:
        IManagerRootService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns an handle to #IApplicationDisplayService (https://switchbrew.org/wiki/Display_services#GetDisplayService)
         */
        void GetDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
