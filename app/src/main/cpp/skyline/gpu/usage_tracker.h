// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/interval_list.h>

namespace skyline::gpu {
    /**
     * @brief Tracks the usage of GPU memory and buffers to allow for fine-grained flushing
     */
    struct UsageTracker {
        IntervalList<u8 *> dirtyIntervals; //!< Intervals of GPU-dirty contents that requires a flush before accessing
        IntervalList<u8 *> sequencedIntervals; //!< Intervals of GPFIFO-sequenced writes that occur within an execution
    };
}
