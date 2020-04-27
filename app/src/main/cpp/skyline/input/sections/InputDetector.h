// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::input {
    /**
     * @brief The structure of an entry for InputDetector (https://switchbrew.org/wiki/HID_Shared_Memory#InputDetectorState)
     */
    struct InputDetectorState {
        u64 globalTimestamp; //!< The global timestamp in samples
        u64 _unk_[0x2];
    };
    static_assert(sizeof(InputDetectorState) == 0x18);

    /**
     * @brief The structure of the InputDetector section (https://switchbrew.org/wiki/HID_Shared_Memory#InputDetector)
     */
    struct InputDetectorSection {
        CommonHeader header; //!< The header for this section
        std::array<InputDetectorState, 2> entries; //!< An array of all of the entries
        u64 _pad_[0x6];
    };
    static_assert(sizeof(InputDetectorSection) == 0x80);
}
