// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::timesrv {
    /**
     * @brief IStaticService (covers time:u, time:a and time:s) is responsible for providing handles to various clock services
     * @url https://switchbrew.org/wiki/PSC_services#time:su.2C_time:s
     */
    class IStaticService : public BaseService {
      public:
        IStaticService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns a handle to a ISystemClock for user time
         */
        Result GetStandardUserSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to a ISystemClock for network time
         */
        Result GetStandardNetworkSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to a ISteadyClock for a steady timepoint
         */
        Result GetStandardSteadyClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to a ITimeZoneService for reading time zone information
         */
        Result GetTimeZoneService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to a ISystemClock for local time
         */
        Result GetStandardLocalSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IStaticService, GetStandardUserSystemClock),
            SFUNC(0x1, IStaticService, GetStandardNetworkSystemClock),
            SFUNC(0x2, IStaticService, GetStandardSteadyClock),
            SFUNC(0x3, IStaticService, GetTimeZoneService),
            SFUNC(0x4, IStaticService, GetStandardLocalSystemClock)
        )
    };
}
