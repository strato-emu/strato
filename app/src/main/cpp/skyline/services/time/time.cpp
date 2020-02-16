#include "timesrv.h"

namespace skyline::service::time {
    time::time(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::time, "time", {
        {0x0, SFUNC(time::GetStandardUserSystemClock)},
        {0x1, SFUNC(time::GetStandardNetworkSystemClock)},
        {0x3, SFUNC(time::GetTimeZoneService)},
        {0x4, SFUNC(time::GetStandardLocalSystemClock)}
    }) {}

    void time::GetStandardUserSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISystemClock>(SystemClockType::User, state, manager), session, response);
    }

    void time::GetStandardNetworkSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISystemClock>(SystemClockType::Network, state, manager), session, response);
    }

    void time::GetTimeZoneService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ITimeZoneService>(state, manager), session, response);
    }

    void time::GetStandardLocalSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISystemClock>(SystemClockType::Local, state, manager), session, response);
    }

    ISystemClock::ISystemClock(SystemClockType clockType, const DeviceState &state, ServiceManager &manager) : type(clockType), BaseService(state, manager, Service::time_ISystemClock, "time:ISystemClock", {
        {0x0, SFUNC(ISystemClock::GetCurrentTime)}
    }) {}

    void ISystemClock::GetCurrentTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u64>(static_cast<u64>(std::time(nullptr)));
    }

    ITimeZoneService::ITimeZoneService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::time_ITimeZoneService, "time:ITimeZoneService", {
        {0x65, SFUNC(ITimeZoneService::ToCalendarTimeWithMyRule)}
    }) {}

    void ITimeZoneService::ToCalendarTimeWithMyRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        time_t curTime = std::time(nullptr);
        tm calender = *std::gmtime(&curTime);
        CalendarTime calendarTime{
            .year = static_cast<u16>(calender.tm_year),
            .month = static_cast<u8>(calender.tm_mon),
            .day = static_cast<u8>(calender.tm_hour),
            .minute = static_cast<u8>(calender.tm_min),
            .second = static_cast<u8>(calender.tm_sec)
        };
        response.Push(calendarTime);
        CalendarAdditionalInfo calendarInfo{
            .day_week = static_cast<u32>(calender.tm_wday),
            .day_month = static_cast<u32>(calender.tm_mday),
            .name = *reinterpret_cast<const u64 *>(calender.tm_zone),
            .dst = static_cast<i32>(calender.tm_isdst),
            .utc_rel = static_cast<u32>(calender.tm_gmtoff)
        };
        response.Push(calendarInfo);
    }
}
