// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::input {
    /**
     * @brief The structure of an entry for Mouse (https://switchbrew.org/wiki/HID_Shared_Memory#MouseState)
     */
    struct MouseState {
        u64 globalTimestamp; //!< The global timestamp in samples
        u64 localTimestamp; //!< The local timestamp in samples

        u32 positionX; //!< The X position of the mouse
        u32 positionY; //!< The Y position of the mouse

        u32 deltaX; //!< The change in the X-axis value
        u32 deltaY; //!< The change in the Y-axis value

        u32 scrollChangeY; //!< The amount scrolled in the Y-axis since the last entry
        u32 scrollChangeX; //!< The amount scrolled in the X-axis since the last entry

        std::bitset<64> buttons; //!< The state of the mouse buttons as a bit-array
    };
    static_assert(sizeof(MouseState) == 0x30);

    /**
     * @brief The structure of the Mouse section (https://switchbrew.org/wiki/HID_Shared_Memory#Mouse)
     */
    struct MouseSection {
        CommonHeader header; //!< The header for this section
        std::array<MouseState, constant::HidEntryCount> entries; //!< An array of all of the entries
        u64 _pad_[0x16];
    };
    static_assert(sizeof(MouseSection) == 0x400);
}
