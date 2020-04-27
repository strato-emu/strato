// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::input {
    /**
     * @brief The structure of an entry for BasicXpad (https://switchbrew.org/wiki/HID_Shared_Memory#BasicXpadState)
     */
    struct BasicXpadState {
        u64 globalTimestamp; //!< The global timestamp in samples
        u64 _unk_[0x4];
    };
    static_assert(sizeof(BasicXpadState) == 0x28);

    /**
     * @brief The structure of the BasicXpad section (https://switchbrew.org/wiki/HID_Shared_Memory#BasicXpad)
     */
    struct BasicXpadSection {
        CommonHeader header; //!< The header for this section
        std::array<BasicXpadState, constant::HidEntryCount> entries; //!< An array of all of the entries
        u64 _pad_[0x27];
    };
    static_assert(sizeof(BasicXpadSection) == 0x400);
}
