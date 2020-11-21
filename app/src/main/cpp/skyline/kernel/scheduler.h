// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline {
    namespace constant {
        constexpr u8 CoreCount{4}; // The amount of cores an HOS process can be scheduled onto (User applications can only be on the first 3 cores, the last one is reserved for the system)
    }

    namespace kernel {
        using CoreMask = std::bitset<constant::CoreCount>;

        /**
         * @brief Priority on HOS determines scheduling behavior relative to other threads
         * @note Lower priority values result in a higher priority, similar to niceness on Linux
         */
        struct Priority {
            u8 min; //!< Numerically lowest priority, highest scheduler priority
            u8 max; //!< Numerically highest priority, lowest scheduler priority

            /**
             * @return A bitmask with each bit corresponding to if scheduler priority with the same index is valid
             */
            constexpr u64 Mask() const {
                return (std::numeric_limits<u64>::max() >> ((std::numeric_limits<u64>::digits - 1 + min) - max)) << min;
            }

            constexpr bool Valid(i8 value) const {
                return (value >= min) && (value <= max);
            }
        };
    }
}
