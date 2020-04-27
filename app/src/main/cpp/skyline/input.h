// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"
#include "kernel/types/KSharedMemory.h"
#include "input/shared_mem.h"

namespace skyline::input {
    /**
     * @brief The Input class manages input devices
     */
    class Input {
      private:
        const DeviceState &state; //!< The state of the device

      public:
        Input(const DeviceState &state);

        std::shared_ptr<npad::CommonNpad> commonNpad; //!< The common npad device
        std::array<std::shared_ptr<npad::NpadDevice>, constant::NpadCount> npad; //!< Array of npad devices

        std::shared_ptr<kernel::type::KSharedMemory> hidKMem; //!< The shared memory reserved for HID input
        HidSharedMemory *hidMem; //!< A pointer to the root of HID shared memory
    };
}
