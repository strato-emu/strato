// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::input {
    /**
     * @url https://switchbrew.org/wiki/HID_Shared_Memory#DebugPadState
     */
    struct DebugPadState {
        u64 timestamp; //!< The total timestamp in ticks
        u8 _unk_[0x20];
    };
    static_assert(sizeof(DebugPadState) == 0x28);

    /**
     * @url https://switchbrew.org/wiki/HID_Shared_Memory#DebugPad
     */
    struct DebugPadSection {
        CommonHeader header;
        std::array<DebugPadState, constant::HidEntryCount> entries;
        u64 _pad_[0x27];
    };
    static_assert(sizeof(DebugPadSection) == 0x400);
}
