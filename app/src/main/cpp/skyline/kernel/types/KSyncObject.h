// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "KObject.h"

namespace skyline::kernel::type {
    /**
     * @brief KSyncObject is an abstract class which holds everything necessary for an object to be synchronizable
     * @note This abstraction is roughly equivalent to KSynchronizationObject on HOS
     */
    class KSyncObject : public KObject {
      public:
        inline static std::mutex syncObjectMutex; //!< A global lock used for locking all signalling to avoid races
        std::list<std::shared_ptr<KThread>> syncObjectWaiters; //!< A list of threads waiting on this object to be signalled
        bool signalled; //!< If the current object is signalled (An object stays signalled till the signal has been explicitly reset)

        /**
         * @param presignalled If this object should be signalled initially or not
         */
        KSyncObject(const DeviceState &state, skyline::kernel::type::KType type, bool presignalled = false) : KObject(state, type), signalled(presignalled) {};

        /**
         * @brief Wakes up any waiters on this object and flips the 'signalled' flag
         */
        void Signal();

        /**
         * @brief Resets the object to an unsignalled state
         * @return If the signal was reset or not
         */
        inline bool ResetSignal() {
            std::lock_guard lock(syncObjectMutex);
            if (signalled) {
                signalled = false;
                return true;
            }
            return false;
        }

        virtual ~KSyncObject() = default;
    };
}
