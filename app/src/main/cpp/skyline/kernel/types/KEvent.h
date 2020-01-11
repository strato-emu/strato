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
