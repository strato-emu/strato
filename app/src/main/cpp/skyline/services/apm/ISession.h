#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::apm {
    /**
     * @brief ISession is a service opened when OpenSession is called by apm for controlling performance
     */
    class ISession : public BaseService {
      private:
        std::array<u32, 2> performanceConfig = {0x00010000, 0x00020001}; //!< This holds the performance config for both handheld(0) and docked(1) mode

      public:
        ISession(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This sets performanceConfig to the given arguments, it doesn't affect anything else (https://switchbrew.org/wiki/PPC_services#SetPerformanceConfiguration)
         */
        void SetPerformanceConfiguration(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This retrieves the particular performanceConfig for a mode and returns it to the client (https://switchbrew.org/wiki/PPC_services#SetPerformanceConfiguration)
         */
        void GetPerformanceConfiguration(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
