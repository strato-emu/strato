#pragma once

#include <kernel/services/base_service.h>
#include <kernel/services/serviceman.h>

namespace skyline::kernel::service::apm {
    /**
     * @brief apm is used to control performance modes of the device, this service however is mostly only used to open an ISession (https://switchbrew.org/wiki/PPC_services#apm)
     */
    class apm : public BaseService {
      public:
        apm(const DeviceState &state, ServiceManager& manager);

        /**
         * @brief This returns an handle to ISession
         */
        void OpenSession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief apm:ISession is a service opened when OpenSession is called by apm
     */
    class ISession : public BaseService {
      private:
        u32 performanceConfig[2] = {0x00010000, 0x00020001}; //!< This holds the performance config for both handheld(0) and docked(1) mode

      public:
        ISession(const DeviceState &state, ServiceManager& manager);

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
