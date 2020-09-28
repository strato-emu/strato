// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

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
     * @brief ISystemClock is used to retrieve and set time
     * @url https://switchbrew.org/wiki/PSC_services#ISystemClock
     */
    class ISystemClock : public BaseService {
      public:
        const SystemClockType type;

        ISystemClock(const SystemClockType clockType, const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns the amount of seconds since epoch
         */
        Result GetCurrentTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the system clock context
         */
        Result GetSystemClockContext(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, ISystemClock, GetCurrentTime),
            SFUNC(0x2, ISystemClock, GetSystemClockContext)
        )
    };
}
