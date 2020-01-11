#pragma once

#include <common.h>
#include "KObject.h"

namespace skyline::kernel::type {
    /**
     * @brief KSyncObject holds the state of a waitable object
     */
    class KSyncObject : public KObject {
      public:
        std::atomic<bool> signalled{false}; //!< If the current object is signalled (Used as object stays signalled till the signal is consumed)

        /**
         * @param state The state of the device
         * @param type The type of the object
         */
        KSyncObject(const DeviceState &state, skyline::kernel::type::KType type) : KObject(state, type) {};

        /**
         * @brief A function for calling when a particular KSyncObject is signalled
         */
        virtual void Signal() {
            signalled = true;
        }
    };
}
