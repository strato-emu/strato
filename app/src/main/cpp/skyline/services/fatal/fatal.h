#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::kernel::service::fatal {
    /**
     * @brief fatal_u is used by applications to throw errors (https://switchbrew.org/wiki/Fatal_services#fatal:u)
     */
    class fatalU : public BaseService {
      public:
        fatalU(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This throws an exception so that emulation will quit
         */
        void ThrowFatal(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
