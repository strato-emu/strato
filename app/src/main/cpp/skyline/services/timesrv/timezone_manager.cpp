// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "results.h"
#include "timezone_manager.h"

namespace skyline::service::timesrv::core {
    Result TimeZoneManager::Setup(std::string_view pLocationName, const SteadyClockTimePoint &pUpdateTime, int pLocationCount, std::array<u8, 0x10> pBinaryVersion, span<u8> binary) {
        SetNewLocation(pLocationName, binary);
        SetUpdateTime(pUpdateTime);
        SetLocationCount(pLocationCount);
        SetBinaryVersion(pBinaryVersion);

        MarkInitialized();
        return {};
    }

    ResultValue<LocationName> TimeZoneManager::GetLocationName() {
        std::scoped_lock lock{mutex};

        if (!IsInitialized())
            return result::ClockUninitialized;

        return locationName;
    }

    Result TimeZoneManager::SetNewLocation(std::string_view pLocationName, span<u8> binary) {
        std::scoped_lock lock{mutex};

        rule = tz_tzalloc(binary.data(), static_cast<long>(binary.size()));
        if (!rule)
            return result::RuleConversionFailed;

        span(locationName).copy_from(pLocationName);

        return {};
    }

    ResultValue<SteadyClockTimePoint> TimeZoneManager::GetUpdateTime() {
        std::scoped_lock lock{mutex};

        if (!IsInitialized())
            return result::ClockUninitialized;

        return updateTime;
    }

    void TimeZoneManager::SetUpdateTime(const SteadyClockTimePoint &pUpdateTime) {
        std::scoped_lock lock{mutex};
        updateTime = pUpdateTime;
    }

    ResultValue<int> TimeZoneManager::GetLocationCount() {
        std::scoped_lock lock{mutex};

        if (!IsInitialized())
            return result::ClockUninitialized;

        return locationCount;
    }

    void TimeZoneManager::SetLocationCount(int pLocationCount) {
        std::scoped_lock lock{mutex};
        locationCount = pLocationCount;
    }

    ResultValue<std::array<u8, 0x10>> TimeZoneManager::GetBinaryVersion() {
        std::scoped_lock lock{mutex};

        if (!IsInitialized())
            return result::ClockUninitialized;

        return binaryVersion;
    }

    void TimeZoneManager::SetBinaryVersion(std::array<u8, 0x10> pBinaryVersion) {
        std::scoped_lock lock{mutex};
        binaryVersion = pBinaryVersion;
    }

    Result TimeZoneManager::ParseTimeZoneBinary(span<u8> binary, span<u8> ruleOut) {
        auto ruleObj{tz_tzalloc(binary.data(), static_cast<long>(binary.size()))};
        if (!ruleObj)
            return result::RuleConversionFailed;

        memcpy(ruleOut.data(), ruleObj, ruleOut.size_bytes());
        tz_tzfree(ruleObj);
        return {};
    }

    ResultValue<FullCalendarTime> TimeZoneManager::ToCalendarTime(tz_timezone_t pRule, PosixTime posixTime) {
        struct tm tmp{};
        auto posixCalendarTime{tz_localtime_rz(pRule, &posixTime, &tmp)};
        if (!posixCalendarTime)
            return result::PermissionDenied; // Not the proper error here but *fine*

        FullCalendarTime out{
            .calendarTime{
                .year = static_cast<u16>(posixCalendarTime->tm_year + 1900),
                .month = static_cast<u8>(posixCalendarTime->tm_mon + 1),
                .day = static_cast<u8>(posixCalendarTime->tm_mday),
                .hour =  static_cast<u8>(posixCalendarTime->tm_hour),
                .minute = static_cast<u8>(posixCalendarTime->tm_min),
                .second = static_cast<u8>(posixCalendarTime->tm_sec),
            },
            .additionalInfo{
                .dayOfWeek = static_cast<u32>(posixCalendarTime->tm_wday),
                .dayOfYear = static_cast<u32>(posixCalendarTime->tm_yday),
                .dst = static_cast<u32>(posixCalendarTime->tm_isdst),
                .gmtOffset = static_cast<i32>(posixCalendarTime->tm_gmtoff),
            },
        };

        std::string_view timeZoneName(posixCalendarTime->tm_zone);
        timeZoneName.copy(out.additionalInfo.timeZoneName.data(), timeZoneName.size());
        return out;
    }

    ResultValue<PosixTime> TimeZoneManager::ToPosixTime(tz_timezone_t pRule, CalendarTime calendarTime) {
        struct tm posixCalendarTime{
            .tm_year = calendarTime.year - 1900,
            .tm_mon = calendarTime.month - 1,
            .tm_mday = calendarTime.day,
            .tm_min = calendarTime.minute,
            .tm_sec = calendarTime.second,
        };

        // Nintendo optionally returns two times here, presumably to deal with DST correction but we are probably fine without it
        return static_cast<PosixTime>(tz_mktime_z(pRule, &posixCalendarTime));
    }
}