// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::timesrv {
    /**
     * @brief ITimeZoneService is used to retrieve and set time
     * @url https://switchbrew.org/wiki/PSC_services#ITimeZoneService
     */
    class ITimeZoneService : public BaseService {
      public:
        ITimeZoneService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Receives a u64 #PosixTime (https://switchbrew.org/wiki/PSC_services#PosixTime), and returns a #CalendarTime (https://switchbrew.org/wiki/PSC_services#CalendarTime), #CalendarAdditionalInfo
         * @url https://switchbrew.org/wiki/PSC_services#CalendarAdditionalInfo
         */
        Result ToCalendarTimeWithMyRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x65, ITimeZoneService, ToCalendarTimeWithMyRule)
        )
    };
}
