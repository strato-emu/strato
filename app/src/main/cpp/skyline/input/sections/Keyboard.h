// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::input {
    /**
     * @brief This enumerates all the modifier keys that can be used
     */
    union ModifierKey {
        u64 raw;
        struct {
            bool lControl : 1; //!< Left Control Key
            bool lShift   : 1; //!< Left Shift Key
            bool lAlt     : 1; //!< Left Alt Key
            bool lWindows : 1; //!< Left Windows Key
            bool rControl : 1; //!< Right Control Key
            bool rShift   : 1; //!< Right Shift Key
            bool rAlt     : 1; //!< Right Alt Key
            bool rWindows : 1; //!< Right Windows Key
            bool capsLock : 1; //!< Caps-Lock Key
            bool scrLock  : 1; //!< Scroll-Lock Key
            bool numLock  : 1; //!< Num-Lock Key
        };
    };

    /**
     * @brief The structure of an entry for Keyboard (https://switchbrew.org/wiki/HID_Shared_Memory#KeyboardState)
     */
    struct KeyboardState {
        u64 globalTimestamp; //!< The global timestamp in samples
        u64 localTimestamp; //!< The local timestamp in samples

        ModifierKey modifers; //!< The state of any modifier keys
        std::bitset<256> keysDown; //!< A bit-array of the state of all the keys
    };
    static_assert(sizeof(KeyboardState) == 0x38);

    /**
     * @brief The structure of the Keyboard section (https://switchbrew.org/wiki/HID_Shared_Memory#Keyboard)
     */
    struct KeyboardSection {
        CommonHeader header;
        std::array<KeyboardState, constant::HidEntryCount> entries;
        u64 _pad_[0x5];
    };
    static_assert(sizeof(KeyboardSection) == 0x400);
}
