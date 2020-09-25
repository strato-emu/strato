// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include "shared_mem.h"

namespace skyline::input {
    /*
     * @brief A description of a point being touched on the screen
     * @note All members are jint as it is treated as a jint array in Kotlin
     * @note This structure corresponds to TouchScreenStateData, see that for details
     */
    struct TouchScreenPoint {
        jint x;
        jint y;
        jint minor;
        jint major;
        jint angle;
    };

    /**
     * @brief This class is used to manage the shared memory responsible for touch-screen data
     */
    class TouchManager {
      private:
        const DeviceState &state;
        bool activated{};
        TouchScreenSection &section;

      public:
        /**
         * @param hid A pointer to HID Shared Memory on the host
         */
        TouchManager(const DeviceState &state, input::HidSharedMemory *hid);

        void Activate();

        void SetState(const span<TouchScreenPoint> &points);
    };
}
