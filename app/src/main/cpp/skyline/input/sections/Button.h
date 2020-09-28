// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::input {
    /**
     * @url https://switchbrew.org/wiki/HID_Shared_Memory#HomeButtonState
     * @url https://switchbrew.org/wiki/HID_Shared_Memory#SleepButtonState
     * @url https://switchbrew.org/wiki/HID_Shared_Memory#CaptureButtonState
     */
    struct ButtonState {
        u64 globalTimestamp; //!< The global timestamp in samples
        u64 _unk_[0x2];
    };
    static_assert(sizeof(ButtonState) == 0x18);

    /**
     * @url https://switchbrew.org/wiki/HID_Shared_Memory#HomeButton
     * @url https://switchbrew.org/wiki/HID_Shared_Memory#SleepButton
     * @url https://switchbrew.org/wiki/HID_Shared_Memory#CaptureButton
     */
    struct ButtonSection {
        CommonHeader header;
        std::array<ButtonState, constant::HidEntryCount> entries;
        u64 _pad_[0x9];
    };
    static_assert(sizeof(ButtonSection) == 0x200);
}
