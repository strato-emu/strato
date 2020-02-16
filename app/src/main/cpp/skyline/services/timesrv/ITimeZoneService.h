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
            u16 year;
            u8 month;
            u8 day;
            u8 hour;
            u8 minute;
            u8 second;
            u8 _pad_;
        };
        static_assert(sizeof(CalendarTime) == 0x8);

        /**
         * @brief This is passed in addition to CalendarTime
         */
        struct CalendarAdditionalInfo {
            u32 day_week;
            u32 day_month;
            u64 name;
            i32 dst;
            u32 utc_rel;
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
