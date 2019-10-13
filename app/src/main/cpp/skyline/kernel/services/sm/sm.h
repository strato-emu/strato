#pragma once

#include <kernel/services/base_service.h>
#include <kernel/services/serviceman.h>

namespace skyline::kernel::service::sm {
    /**
     * @brief sm: or Service Manager is responsible for providing handles to services (https://switchbrew.org/wiki/Services_API)
     */
    class sm : public BaseService {
      public:
        sm(const DeviceState &state, ServiceManager& manager);

        /**
         * @brief This initializes the sm: service. It doesn't actually do anything. (https://switchbrew.org/wiki/Services_API#Initialize)
         */
        void Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a handle to a service with it's name passed in as an argument (https://switchbrew.org/wiki/Services_API#GetService)
         */
        void GetService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
