// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::timesrv {
    /**
     * @brief ITimeZoneService is used to retrieve and set time (https://switchbrew.org/wiki/PSC_services#ITimeZoneService)
     */
    class ITimeZoneService : public BaseService {
      private:
        /**
         * @brief This holds a particular time point in calendar format
         */
        struct CalendarTime {
            u16 year; //!< The Year component of the date
            u8 month; //!< The Month component of the date
            u8 day; //!< The Day component of the date
            u8 hour; //!< The Hour component of the date
            u8 minute; //!< The Minute component of the date
            u8 second; //!< The Second component of the date
            u8 _pad_;
        };
        static_assert(sizeof(CalendarTime) == 0x8);

        /**
         * @brief This is passed in addition to CalendarTime
         */
        struct CalendarAdditionalInfo {
            u32 dayWeek; //!< The amount of days since Sunday
            u32 dayMonth; //!< The amount of days since the start of the month
            u64 name; //!< The name of the time zone
            i32 dst; //!< If DST is in effect or not
            u32 utcRel; //!< The offset of the time from GMT in seconds
        };
        static_assert(sizeof(CalendarAdditionalInfo) == 0x18);

      public:
        ITimeZoneService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This receives a u64 #PosixTime (https://switchbrew.org/wiki/PSC_services#PosixTime), and returns a #CalendarTime (https://switchbrew.org/wiki/PSC_services#CalendarTime), #CalendarAdditionalInfo (https://switchbrew.org/wiki/PSC_services#CalendarAdditionalInfo)
         */
        void ToCalendarTimeWithMyRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
