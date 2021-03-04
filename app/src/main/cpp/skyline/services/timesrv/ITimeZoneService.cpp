// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "common.h"
#include "core.h"
#include "ITimeZoneService.h"

namespace skyline::service::timesrv {
    ITimeZoneService::ITimeZoneService(const DeviceState &state, ServiceManager &manager, core::TimeServiceObject &core, bool writeable) : BaseService(state, manager), core(core), writeable(writeable) {}

    Result ITimeZoneService::GetDeviceLocationName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto locationName{core.timeZoneManager.GetLocationName()};
        if (locationName)
            response.Push(*locationName);

        return locationName;
    }

    Result ITimeZoneService::SetDeviceLocationName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!writeable)
            return result::PermissionDenied;

        return result::Unimplemented;
    }

    Result ITimeZoneService::GetTotalLocationNameCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto count{core.timeZoneManager.GetLocationCount()};
        if (count)
            response.Push<i32>(*count);

        return count;
    }

    Result ITimeZoneService::LoadLocationNameList(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return result::Unimplemented;
    }

    Result ITimeZoneService::LoadTimeZoneRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return result::Unimplemented;
    }

    Result ITimeZoneService::GetTimeZoneRuleVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto version{core.timeZoneManager.GetBinaryVersion()};
        if (version)
            response.Push(*version);

        return version;
    }

    Result ITimeZoneService::GetDeviceLocationNameAndUpdatedTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto locationName{core.timeZoneManager.GetLocationName()};
        if (!locationName)
            return locationName;

        auto updateTime{core.timeZoneManager.GetUpdateTime()};
        if (!updateTime)
            return updateTime;

        response.Push(*locationName);
        response.Push<u32>(0); // Padding
        response.Push(*updateTime);

        return {};
    }

    Result ITimeZoneService::SetDeviceLocationNameWithTimeZoneBinaryIpc(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto locationName{request.Pop<LocationName>()};

        return SetDeviceLocationNameWithTimeZoneBinary(span(locationName).as_string(true), request.inputBuf.at(0));
    }

    Result ITimeZoneService::SetDeviceLocationNameWithTimeZoneBinary(std::string_view locationName, span<u8> binary) {
        if (!writeable)
            return result::PermissionDenied;

        return core.timeZoneManager.SetNewLocation(locationName, binary);
    }

    Result ITimeZoneService::ParseTimeZoneBinaryIpc(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core::TimeZoneManager::ParseTimeZoneBinary(request.inputBuf.at(0), request.outputBuf.at(0));
    }

    Result ITimeZoneService::ParseTimeZoneBinary(span<u8> binary, span<u8> rule) {
        return core::TimeZoneManager::ParseTimeZoneBinary(binary, rule);
    }

    Result ITimeZoneService::GetDeviceLocationNameOperationEventReadableHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return result::Unimplemented;
    }

    Result ITimeZoneService::ToCalendarTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto posixTime{request.Pop<PosixTime>()};
        auto calendarTime{core::TimeZoneManager::ToCalendarTime(reinterpret_cast<tz_timezone_t>(request.inputBuf.at(0).data()), posixTime)};

        if (calendarTime)
            response.Push(*calendarTime);

        return calendarTime;
    }

    Result ITimeZoneService::ToCalendarTimeWithMyRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto posixTime{request.Pop<PosixTime>()};
        auto calendarTime{core.timeZoneManager.ToCalendarTimeWithMyRule(posixTime)};

        if (calendarTime)
            response.Push(*calendarTime);

        return calendarTime;
    }

    Result ITimeZoneService::ToPosixTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto calendarTime{request.Pop<CalendarTime>()};
        auto posixTime{core::TimeZoneManager::ToPosixTime(reinterpret_cast<tz_timezone_t>(request.inputBuf.at(0).data()), calendarTime)};
        if (!posixTime)
            return posixTime;

        request.outputBuf.at(0).as<PosixTime>() = *posixTime;
        response.Push<u32>(1);
        return {};
    }

    Result ITimeZoneService::ToPosixTimeWithMyRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto calendarTime{request.Pop<CalendarTime>()};
        auto posixTime{core.timeZoneManager.ToPosixTimeWithMyRule(calendarTime)};
        if (!posixTime)
            return posixTime;

        request.outputBuf.at(0).as<PosixTime>() = *posixTime;
        response.Push<u32>(1);
        return {};
    }
}
