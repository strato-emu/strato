// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::input {
    /**
     * @brief This enumerates all the modifier keys that can be used
     */
    union ModifierKey {
        struct {
            bool LControl : 1; //!< Left Control Key
            bool LShift   : 1; //!< Left Shift Key
            bool LAlt     : 1; //!< Left Alt Key
            bool LWindows : 1; //!< Left Windows Key
            bool RControl : 1; //!< Right Control Key
            bool RShift   : 1; //!< Right Shift Key
            bool RAlt     : 1; //!< Right Alt Key
            bool RWindows : 1; //!< Right Windows Key
            bool CapsLock : 1; //!< Caps-Lock Key
            bool ScrLock  : 1; //!< Scroll-Lock Key
            bool NumLock  : 1; //!< Num-Lock Key
        };
        u64 raw;
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
