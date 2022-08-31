// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"
#include "kernel/types/KSharedMemory.h"
#include "input/shared_mem.h"
#include "input/npad.h"
#include "input/touch.h"

namespace skyline::input {
    /**
     * @brief The Input class manages components responsible for translating host input to guest input
     */
    class Input {
      private:
        const DeviceState &state;

      public:
        std::shared_ptr<kernel::type::KSharedMemory> kHid; //!< The kernel shared memory object for HID Shared Memory
        HidSharedMemory *hid; //!< A pointer to HID Shared Memory on the host

        NpadManager npad;
        TouchManager touch;

        Input(const DeviceState &state);

      private:
        std::thread updateThread; //!< A thread that handles delivering HID shared memory updates at a fixed rate

        /**
         * @brief The entry point for the update thread, this handles timing and delegation to the shared memory managers
         */
        void UpdateThread();
    };
}
