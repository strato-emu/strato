// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::input {
    /**
     * @brief The structure of an entry for Gesture (https://switchbrew.org/wiki/HID_Shared_Memory#GestureState)
     */
    struct GestureState {
        u64 globalTimestamp; //!< The global timestamp in samples
        u64 _unk_[0xC];
    };
    static_assert(sizeof(GestureState) == 0x68);

    /**
     * @brief The structure of the Gesture section (https://switchbrew.org/wiki/HID_Shared_Memory#Gesture)
     */
    struct GestureSection {
        CommonHeader header;
        std::array<GestureState, constant::HidEntryCount> entries;
        u64 _pad_[0x1F];
    };
    static_assert(sizeof(GestureSection) == 0x800);
}
