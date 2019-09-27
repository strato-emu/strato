#pragma once

#include "KSyncObject.h"

namespace skyline::kernel::type {
    /**
     * @brief KEvent is an object that's signalled on an repeatable event occurring (https://switchbrew.org/wiki/Kernel_objects#KEvent)
     */
    class KEvent : public KSyncObject {
      public:
        /**
         * @param handle The handle of the object in the handle table
         * @param pid The PID of the main thread
         * @param state The state of the device
         */
        KEvent(skyline::handle_t handle, pid_t pid, const DeviceState &state) : KSyncObject(handle, pid, state, KType::KEvent) {}
    };
}
