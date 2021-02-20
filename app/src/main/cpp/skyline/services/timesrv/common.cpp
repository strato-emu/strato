#include <common.h>
#include "results.h"
#include "common.h"

namespace skyline::service::timesrv {
    ResultValue<i64> ClockSnapshot::GetCurrentTime(const SteadyClockTimePoint &timePoint, const SystemClockContext &context) {
        if (context.timestamp.clockSourceId != timePoint.clockSourceId)
            return result::ClockSourceIdMismatch;

        return context.offset + timePoint.timePoint;
    }

    ResultValue<i64> GetSpanBetween(const SteadyClockTimePoint &start, const SteadyClockTimePoint &end) {
        // We can't compare between different clocks as they don't necessarily operate from the same origin
        if (start.clockSourceId != end.clockSourceId)
            return result::InvalidComparison;

        if (((start.timePoint > 0) && (end.timePoint < std::numeric_limits<i64>::min() + start.timePoint)) ||
            ((start.timePoint < 0) && (end.timePoint > std::numeric_limits<i64>::max() + start.timePoint)))
            return result::CompareOverflow;

        return end.timePoint - start.timePoint;
    }
}