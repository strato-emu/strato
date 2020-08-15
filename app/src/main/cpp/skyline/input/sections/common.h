// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <array>
#include <common.h>

namespace skyline {
    namespace constant {
        constexpr u8 HidEntryCount = 17; //!< The amount of entries in each HID device
        constexpr u8 NpadCount = 10; //!< The amount of NPads in shared memory
        constexpr u8 ControllerCount = 8; //!< The maximum amount of host controllers
        constexpr u32 NpadBatteryFull = 2; //!< The full battery state of an npad
    }

    namespace input {
        /**
         * @brief This is the common part of the header for all sections
         */
        struct CommonHeader {
            u64 timestamp; //!< The timestamp of the latest entry in ticks
            u64 entryCount{constant::HidEntryCount}; //!< The number of entries (17)
            u64 currentEntry; //!< The index of the latest entry
            u64 maxEntry{constant::HidEntryCount - 1}; //!< The maximum entry index (16)
        };
        static_assert(sizeof(CommonHeader) == 0x20);
    }
}
