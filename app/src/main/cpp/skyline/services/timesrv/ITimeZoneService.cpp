#include "ITimeZoneService.h"

namespace skyline::service::timesrv {
    ITimeZoneService::ITimeZoneService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::timesrv_ITimeZoneService, "timesrv:ITimeZoneService", {
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
