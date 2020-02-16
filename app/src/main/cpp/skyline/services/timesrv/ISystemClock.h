#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::timesrv {
    /**
     * @brief The type of a #SystemClockType
     */
    enum class SystemClockType {
        User, //!< Use time provided by user
        Network, //!< Use network time
        Local, //!< Use local time
    };

    /**
     * @brief ISystemClock is used to retrieve and set time (https://switchbrew.org/wiki/PSC_services#ISystemClock)
     */
    class ISystemClock : public BaseService {
      public:
        const SystemClockType type; //!< The type of the system clock

        ISystemClock(const SystemClockType clockType, const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns the amount of seconds since epoch
         */
        void GetCurrentTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
