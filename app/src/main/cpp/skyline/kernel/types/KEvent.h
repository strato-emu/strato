// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "KSyncObject.h"

namespace skyline::kernel::type {
    /**
     * @brief KEvent is an object that's signalled on an repeatable event occurring
     * @url https://switchbrew.org/wiki/Kernel_objects#KEvent
     */
    class KEvent : public KSyncObject {
      public:
        /**
         * @param presignalled If this object should be signalled initially or not
         */
        KEvent(const DeviceState &state, bool presignalled) : KSyncObject(state, KType::KEvent, presignalled) {}
    };
}
