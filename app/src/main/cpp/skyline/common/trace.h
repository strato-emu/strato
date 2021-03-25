#pragma once

#include <limits>
#include <perfetto.h>
#include <common.h>

#define TRACE_EVENT_FMT(category, formatString, ...) TRACE_EVENT(category, nullptr, [&](perfetto::EventContext ctx) { \
    ctx.event()->set_name(skyline::util::Format(formatString, __VA_ARGS__));                                          \
})

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("scheduler").SetDescription("Events from the HLE scheduler"),
    perfetto::Category("kernel").SetDescription("Events from parts of the HLE kernel"),
    perfetto::Category("guest").SetDescription("Events relating to guest code"),
    perfetto::Category("gpu").SetDescription("Events from the emulated GPU"),
    perfetto::Category("service").SetDescription("Events from the HLE sysmodule implementations")
);

namespace skyline::trace {
    /**
     * @brief Perfetto track IDs for custom tracks, counting down from U64 max to avoid conflicts
     */
    enum class TrackIds : u64 {
        Presentation = std::numeric_limits<u64>::max(),
    };
}
