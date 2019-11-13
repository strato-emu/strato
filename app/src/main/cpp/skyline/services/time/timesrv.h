#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::kernel::service::time {
    /**
     * @brief The type of a #SystemClockType
     */
    enum class SystemClockType {
        User, //!< Use time provided by user
        Network, //!< Use network time
        Local, //!< Use local time
    };

    /**
     * @brief time (This covers both time:a and time:s) is responsible for providing handles to various clock services (https://switchbrew.org/wiki/PSC_services#time:su.2C_time:s)
     */
    class time : public BaseService {
      public:
        time(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns a handle to a ISystemClock for user time (https://switchbrew.org/wiki/Services_API#GetStandardUserSystemClock)
         */
        void GetStandardUserSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a handle to a ISystemClock for user time (https://switchbrew.org/wiki/Services_API#GetStandardNetworkSystemClock)
         */
        void GetStandardNetworkSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a handle to a ISystemClock for user time (https://switchbrew.org/wiki/Services_API#GetStandardNetworkSystemClock)
         */
        void GetTimeZoneService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a handle to a ISystemClock for user time (https://switchbrew.org/wiki/Services_API#GetStandardNetworkSystemClock)
         */
        void GetStandardLocalSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief ISystemClock is used to retrieve and set time (https://switchbrew.org/wiki/PSC_services#ISystemClock)
     */
    class ISystemClock : public BaseService {
      public:
        SystemClockType type; //!< The type of the system clock

        ISystemClock(SystemClockType clockType, const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns the amount of seconds since epoch
         */
        void GetCurrentTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief ITimeZoneService is used to retrieve and set time (https://switchbrew.org/wiki/PSC_services#ITimeZoneService)
     */
    class ITimeZoneService : public BaseService {
      public:
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
            u8 : 8;
        };
        static_assert(sizeof(CalendarTime) == 8);

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
        static_assert(sizeof(CalendarAdditionalInfo) == 24);

        ITimeZoneService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This receives a u64 #PosixTime (https://switchbrew.org/wiki/PSC_services#PosixTime), and returns a #CalendarTime (https://switchbrew.org/wiki/PSC_services#CalendarTime), #CalendarAdditionalInfo (https://switchbrew.org/wiki/PSC_services#CalendarAdditionalInfo)
         */
        void ToCalendarTimeWithMyRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
