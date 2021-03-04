// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <horizon_time.h>
#include <common.h>
#include "common.h"

namespace skyline::service::timesrv::core {
    /**
     * @brief TimeZoneManager handles converting POSIX times to calendar times and vice-versa by using a rule struct
     */
    class TimeZoneManager {
      private:
        bool initialized{};
        std::mutex mutex;
        tz_timezone_t rule{}; //!< Rule corresponding to the timezone that is currently in use
        SteadyClockTimePoint updateTime{}; //!< Time when the rule was last updated
        int locationCount{}; //!< The number of possible timezone binary locations
        std::array<u8, 0x10> binaryVersion{}; //!< The version of the tzdata package
        LocationName locationName{}; //!< Name of the currently selected location

        void MarkInitialized() {
            initialized = true;
        }

      public:
        bool IsInitialized() {
            return initialized;
        }

        /**
         * @brief Initialises the manager, setting the initial timezone so it is ready for use by applications
         */
        Result Setup(std::string_view pLocationName, const SteadyClockTimePoint &pUpdateTime, int pLocationCount, std::array<u8, 0x10> pBinaryVersion, span<u8> binary);

        ResultValue<LocationName> GetLocationName();

        /**
         * @brief Parses the given binary into rule and sets the appropriate location name
         */
        Result SetNewLocation(std::string_view pLocationName, span<u8> binary);

        ResultValue<SteadyClockTimePoint> GetUpdateTime();

        void SetUpdateTime(const SteadyClockTimePoint &pUpdateTime);

        ResultValue<int> GetLocationCount();

        void SetLocationCount(int pLocationCount);

        ResultValue<std::array<u8, 0x10>> GetBinaryVersion();

        void SetBinaryVersion(std::array<u8, 0x10> pBinaryVersion);

        /**
         * @brief Parses a raw TZIF2 file into a timezone rule that can be passed to other functions
         */
        static Result ParseTimeZoneBinary(span<u8> binary, span<u8> ruleOut);

        /**
         * @brief Converts a POSIX time to a calendar time using the given rule
         */
        static ResultValue<FullCalendarTime> ToCalendarTime(tz_timezone_t pRule, PosixTime posixTime);

        /**
         * @brief Converts a POSIX to a calendar time using the current location's rule
         */
        ResultValue<FullCalendarTime> ToCalendarTimeWithMyRule(PosixTime posixTime) {
            return ToCalendarTime(rule, posixTime);
        }

        /**
         * @brief Converts a calendar time to a POSIX time using the given rule
         */
        static ResultValue<PosixTime> ToPosixTime(tz_timezone_t pRule, CalendarTime calendarTime);

        /**
         * @brief Converts a calendar time to a POSIX time using the current location's rule
         */
        ResultValue<PosixTime> ToPosixTimeWithMyRule(CalendarTime calendarTime) {
            return ToPosixTime(rule, calendarTime);
        }
    };
}