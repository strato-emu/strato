#pragma once

#include <common.h>
#include <common/uuid.h>

namespace skyline::service::timesrv {
    using PosixTime = i64; //!< Unit for time in seconds since the epoch
    using LocationName = std::array<char, 0x24>;

    /**
     * @brief Stores a quantity of time with nanosecond accuracy and provides helper functions to convert it to other units
     */
    class TimeSpanType {
      private:
        i64 ns{}; //!< Timepoint of the timespan in nanoseconds

      public:
        constexpr TimeSpanType() {}

        constexpr TimeSpanType(i64 ns) : ns(ns) {}

        static constexpr TimeSpanType FromNanoseconds(i64 ns) {
            return {ns};
        }

        static constexpr TimeSpanType FromSeconds(i64 s) {
            return {s * skyline::constant::NsInSecond};
        }

        static constexpr TimeSpanType FromDays(i64 d) {
            return {d * skyline::constant::NsInDay};
        }

        constexpr i64 Nanoseconds() const {
            return ns;
        }

        constexpr i64 Microseconds() const {
            return ns / skyline::constant::NsInMicrosecond;
        }

        constexpr i64 Seconds() const {
            return ns / skyline::constant::NsInSecond;
        }

        constexpr friend bool operator>(const TimeSpanType &lhs, const TimeSpanType &rhs) {
            return lhs.ns > rhs.ns;
        }

        constexpr friend bool operator<(const TimeSpanType &lhs, const TimeSpanType &rhs) {
            return lhs.ns < rhs.ns;
        }

        constexpr friend TimeSpanType operator+(const TimeSpanType &lhs, const TimeSpanType &rhs) {
            return FromNanoseconds(lhs.ns + rhs.ns);
        }

        constexpr friend TimeSpanType operator-(const TimeSpanType &lhs, const TimeSpanType &rhs) {
            return FromNanoseconds(lhs.ns - rhs.ns);
        }
    };

    /*
     * Using __attribute__((packed)) doesn't work in new NDKs when a struct contains 128-bit integer members, likely because of a ndk/compiler bug
     * Use #pragma pack to ensure that the following structs are tightly packed
     */
    #pragma pack(1)

    /**
     * @brief Holds details about a point in time sourced from a steady clock (e.g. RTC)
     * @url https://switchbrew.org/w/index.php?title=PSC_services#SteadyClockTimePoint
     */
    struct SteadyClockTimePoint {
        i64 timePoint; //!< Time in seconds
        UUID clockSourceId; //!< The UUID of the steady clock this timepoint comes from

        auto operator<=>(const SteadyClockTimePoint &) const = default;
    };
    static_assert(sizeof(SteadyClockTimePoint) == 0x18);

    /**
     * @brief Describes a system clocks offset from its associated steady clock
     * @url https://switchbrew.org/w/index.php?title=PSC_services#SystemClockContext
     */
    struct SystemClockContext {
        i64 offset; // Offset between the steady timepoint and the epoch
        SteadyClockTimePoint timestamp; //!< The steady timepoint this context was calibrated from

        auto operator<=>(const SystemClockContext &) const = default;
    };
    static_assert(sizeof(SystemClockContext) == 0x20);

    #pragma pack()

    /**
     * @brief A particular time point in Nintendo's calendar format
     * @url https://switchbrew.org/w/index.php?title=PSC_services#CalendarTime
     */
    struct CalendarTime {
        u16 year; //!< The current year minus 1900
        u8 month; //!< 1-12 (POSIX time uses 0-11)
        u8 day; //!< 1-31
        u8 hour; //!< 0-23
        u8 minute; //!< 0-59
        u8 second; //!< 0-60
        u8 _pad_;
    };
    static_assert(sizeof(CalendarTime) == 0x8);

    /**
     * @brief Additional metadata about the time alongside CalendarTime
     * @url https://switchbrew.org/w/index.php?title=PSC_services#CalendarAdditionalInfo
     */
    struct CalendarAdditionalInfo {
        u32 dayOfWeek; //!< 0-6
        u32 dayOfYear; //!< 0-365
        std::array<char, 8> timeZoneName;
        u32 dst; //!< If DST is in effect or not
        i32 gmtOffset; //!< Offset of the time from GMT in seconds
    };
    static_assert(sizeof(CalendarAdditionalInfo) == 0x18);

    /**
     * @brief Returned by ToCalendarTime and contains all details about a time
     */
    struct FullCalendarTime {
        CalendarTime calendarTime;
        CalendarAdditionalInfo additionalInfo;
    };
    static_assert(sizeof(FullCalendarTime) == 0x20);

    /**
     * @brief A snapshot of all clocks in the system
     */
    struct ClockSnapshot {
        SystemClockContext userContext;
        SystemClockContext networkContext;
        PosixTime userPosixTime;
        PosixTime networkPosixTime;
        CalendarTime userCalendarTime;
        CalendarTime networkCalendarTime;
        CalendarAdditionalInfo userCalendarAdditionalInfo;
        CalendarAdditionalInfo networkCalendarAdditionalInfo;
        SteadyClockTimePoint steadyClockTimePoint;
        LocationName locationName;
        u8 automaticCorrectionEnabled;
        u8 _unk_;
        u16 version;

        /**
         * @brief Gets the current time based off of the supplied timepoint and context
         */
        static ResultValue<PosixTime> GetCurrentTime(const SteadyClockTimePoint &timePoint, const SystemClockContext &context);
    };
    static_assert(sizeof(ClockSnapshot) == 0xD0);

    /**
     * @brief Gets the time between a pair of steady clock timepoints
     */
    ResultValue<i64> GetSpanBetween(const SteadyClockTimePoint &start, const SteadyClockTimePoint &end);
}