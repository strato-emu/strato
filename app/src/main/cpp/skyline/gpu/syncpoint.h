// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline {
    namespace constant {
        constexpr size_t MaxHwSyncpointCount = 192;
    }

    namespace gpu {
        class Syncpoint {
          private:
            /**
             * @brief This holds information about a single waiter on a syncpoint
             */
            struct Waiter {
                u32 threshold;
                std::function<void()> callback;
            };

            Mutex waiterLock{}; //!< Locks insertions and deletions of waiters
            std::map<u64, Waiter> waiterMap{};
            u64 nextWaiterId{1};

          public:
            std::atomic<u32> value{};

            /**
             * @brief Registers a new waiter with a callback that will be called when the syncpoint reaches the target threshold
             * @note The callback will be called immediately if the syncpoint has already reached the given threshold
             * @return A persistent identifier that can be used to refer to the waiter, or 0 if the threshold has already been reached
             */
            u64 RegisterWaiter(u32 threshold, const std::function<void()> &callback);

            /**
             * @brief Removes a waiter given by 'id' from the pending waiter map
             */
            void DeregisterWaiter(u64 id);

            /**
             * @brief Increments the syncpoint by 1
             * @return The new value of the syncpoint
             */
            u32 Increment();

            /**
             * @brief Waits for the syncpoint to reach given threshold
             * @return false if the timeout was reached, otherwise true
             */
            bool Wait(u32 threshold, std::chrono::steady_clock::duration timeout);
        };
    }
}
