#pragma once

#include <limits>
#include <perfetto.h>
#include <common.h>

PERFETTO_DEFINE_CATEGORIES(perfetto::Category("sched").SetDescription("Events from the scheduler"),
                             perfetto::Category("kernel").SetDescription("Events from parts of the kernel"),
                             perfetto::Category("guest").SetDescription("Events relating to guest code"),
                             perfetto::Category("gpu").SetDescription("Events from the emulated GPU"));

namespace skyline::tracing {
    /**
     * @brief Perfetto track IDs for custom tracks, counting down from U64 max to avoid conflicts
     */
    enum class TrackIds : u64 {
        Presentation = std::numeric_limits<u64>::max()
    };
}