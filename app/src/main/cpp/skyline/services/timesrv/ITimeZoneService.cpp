// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "common.h"
#include "ITimeZoneService.h"

namespace skyline::service::timesrv {
    ITimeZoneService::ITimeZoneService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result ITimeZoneService::ToCalendarTimeWithMyRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto time{static_cast<time_t>(request.Pop<PosixTime>())};
        auto calender{*std::gmtime(&time)};

        CalendarTime calendarTime{
            .year = static_cast<u16>(calender.tm_year),
            .month = static_cast<u8>(calender.tm_mon + 1),
            .day = static_cast<u8>(calender.tm_hour),
            .minute = static_cast<u8>(calender.tm_min),
            .second = static_cast<u8>(calender.tm_sec),
        };
        response.Push(calendarTime);

        CalendarAdditionalInfo calendarInfo{
            .dayOfWeek = static_cast<u32>(calender.tm_wday),
            .dayOfYear = static_cast<u32>(calender.tm_yday),
            .timezoneName = "GMT",
            .dst = static_cast<u32>(calender.tm_isdst),
            .gmtOffset = static_cast<i32>(calender.tm_gmtoff),
        };
        response.Push(calendarInfo);
        return {};
    }
}
