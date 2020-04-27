// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::input {
    /**
     * @brief This structure holds information about a single touch point
     */
    struct TouchScreenStateData {
        u64 timestamp; //!< The timestamp in samples
        u32 _pad0_;

        u32 index; //!< The index of this touch

        u32 positionX; //!< The X position of this touch
        u32 positionY; //!< The Y position of this touch

        u32 diameterX; //!< The diameter of the touch on the X-axis
        u32 diameterY; //!< The diameter of the touch on the Y-axis

        u32 angle; //!< The angle of the touch
        u32 _pad1_;
    };
    static_assert(sizeof(TouchScreenStateData) == 0x28);

    /**
     * @brief The structure of an entry for TouchScreen (https://switchbrew.org/wiki/HID_Shared_Memory#TouchScreenState)
     */
    struct TouchScreenState {
        u64 globalTimestamp; //!< The global timestamp in samples
        u64 localTimestamp; //!< The local timestamp in samples

        u64 touchCount; //!< The amount of active touch instances
        std::array<TouchScreenStateData, 16> data;
    };
    static_assert(sizeof(TouchScreenState) == 0x298);

    /**
     * @brief The structure of the TouchScreen section (https://switchbrew.org/wiki/HID_Shared_Memory#TouchScreen)
     */
    struct TouchScreenSection {
        CommonHeader header; //!< The header for this section
        std::array<TouchScreenState, constant::HidEntryCount> entries; //!< An array of all of the entries
        u64 _pad_[0x79];
    };
    static_assert(sizeof(TouchScreenSection) == 0x3000);
}
