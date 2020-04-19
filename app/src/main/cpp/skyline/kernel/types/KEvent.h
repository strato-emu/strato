// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "KSyncObject.h"

namespace skyline::kernel::type {
    /**
     * @brief KEvent is an object that's signalled on an repeatable event occurring (https://switchbrew.org/wiki/Kernel_objects#KEvent)
     */
    class KEvent : public KSyncObject {
      public:
        /**
         * @param state The state of the device
         */
        KEvent(const DeviceState &state) : KSyncObject(state, KType::KEvent) {}

        /**
         * @brief Resets the KEvent to an unsignalled state
         */
        inline void ResetSignal() {
            signalled = false;
        }
    };
}
