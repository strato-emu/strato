#pragma once

#include <common.h>
#include "KObject.h"

namespace skyline::kernel::type {
  /**
   * @brief KSyncObject holds the state of a waitable object
   */
    class KSyncObject : public KObject {
      public:
        bool signalled = false; //!< If the current object is signalled (Used by KEvent as it stays signalled till svcClearEvent or svcClearSignal is called)
        std::vector<pid_t> waitThreads; //!< A vector of threads waiting on this object

        /**
         * @param state The state of the device
         * @param type The type of the object
         */
        KSyncObject(const DeviceState &state, skyline::kernel::type::KType type);

        /**
         * @brief A function for calling when a particular KSyncObject is signalled
         */
        virtual void Signal();
    };
}
