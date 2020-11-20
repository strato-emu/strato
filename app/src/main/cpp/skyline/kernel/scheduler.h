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

        struct Priority {
            u8 min;
            u8 max;

            constexpr u64 Mask() const {
                u64 mask{};
                for (u8 i{min}; i <= max; i++)
                    mask |= 1 << i;
                return mask;
            }

            constexpr bool Valid(i8 value) const {
                return (value >= min) && (value <= max);
            }
        };
    }
}
