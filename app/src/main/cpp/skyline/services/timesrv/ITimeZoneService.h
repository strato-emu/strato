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
      private:
        /**
         * @brief A particular time point in Nintendo's calendar format
         */
        struct CalendarTime {
            u16 year; //!< Amount of years that have passed since 1900
            u8 month; //!< Month of the year (1-12) [POSIX time uses 0-11]
            u8 day; //!< Day of the month (1-31)
            u8 hour; //!< Hour of the day (0-23)
            u8 minute; //!< Minute of the hour (0-59)
            u8 second; //!< Second of the minute (0-60)
            u8 _pad_;
        };
        static_assert(sizeof(CalendarTime) == 0x8);

        /**
         * @brief Additional metadata about the time alongside CalendarTime
         */
        struct CalendarAdditionalInfo {
            u32 dayWeek; //!< Amount of days since Sunday
            u32 dayMonth; //!< Amount of days since the start of the month
            u64 tzName; //!< The name of the time zone
            i32 dst; //!< If DST is in effect or not
            u32 utcRel; //!< Offset of the time from GMT in seconds
        };
        static_assert(sizeof(CalendarAdditionalInfo) == 0x18);

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
