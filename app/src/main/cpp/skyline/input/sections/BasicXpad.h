// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::input {
    /**
     * @url https://switchbrew.org/wiki/HID_Shared_Memory#BasicXpadState
     */
    struct BasicXpadState {
        u64 globalTimestamp; //!< The global timestamp in samples
        u64 _unk_[0x4];
    };
    static_assert(sizeof(BasicXpadState) == 0x28);

    /**
     * @url https://switchbrew.org/wiki/HID_Shared_Memory#BasicXpad
     */
    struct BasicXpadSection {
        CommonHeader header;
        std::array<BasicXpadState, constant::HidEntryCount> entries;
        u64 _pad_[0x27];
    };
    static_assert(sizeof(BasicXpadSection) == 0x400);
}
