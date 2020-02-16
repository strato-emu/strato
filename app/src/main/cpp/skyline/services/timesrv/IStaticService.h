#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::timesrv {
    /**
     * @brief IStaticService (This covers time:u, time:a and time:s) is responsible for providing handles to various clock services (https://switchbrew.org/wiki/PSC_services#time:su.2C_time:s)
     */
    class IStaticService : public BaseService {
      public:
        IStaticService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns a handle to a ISystemClock for user time
         */
        void GetStandardUserSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a handle to a ISystemClock for network time
         */
        void GetStandardNetworkSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a handle to a ITimeZoneService for reading time zone information
         */
        void GetTimeZoneService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a handle to a ISystemClock for local time
         */
        void GetStandardLocalSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
