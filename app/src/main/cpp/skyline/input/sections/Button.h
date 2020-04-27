// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::input {
    /**
     * @brief The state structure for all Button sections (HomeButton, SleepButton, CaptureButton)
     */
    struct ButtonState {
        u64 globalTimestamp; //!< The global timestamp in samples
        u64 _unk_[0x2];
    };
    static_assert(sizeof(ButtonState) == 0x18);

    /**
     * @brief The section structure for all Button sections (HomeButton, SleepButton, CaptureButton)
     */
    struct ButtonSection {
        CommonHeader header; //!< The header for this section
        std::array<ButtonState, constant::HidEntryCount> entries; //!< An array of all of the entries
        u64 _pad_[0x9];
    };
    static_assert(sizeof(ButtonSection) == 0x200);
}
