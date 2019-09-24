#pragma once

#include "../../common.h"
#include "KObject.h"

namespace skyline::kernel::type {
  /**
   * @brief KSyncObject holds the state of a waitable object
   */
    class KSyncObject : public KObject {
      public:
        /**
         * @param handle The handle of the object in the handle table
         * @param pid The PID of the main thread
         * @param state The state of the device
         * @param type The type of the object
         */
        KSyncObject(skyline::handle_t handle, pid_t pid, const DeviceState &state, skyline::kernel::type::KType type);

        // TODO: Rewrite this so that we store list of waiting threads instead
        /**
         * @brief A function for calling when a particular KSyncObject is signalled
         */
        void Signal();
    };
}
