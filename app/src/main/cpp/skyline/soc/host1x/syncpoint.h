// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2020 Ryujinx Team and Contributors (https://github.com/Ryujinx/)

#pragma once

#include <common.h>

namespace skyline::soc::host1x {
    constexpr size_t SyncpointCount{192}; //!< The number of host1x syncpoints on T210

    /**
     * @brief The Syncpoint class represents a single syncpoint in the GPU which is used for GPU -> CPU synchronisation
     */
    class Syncpoint {
      private:
        std::atomic<u32> value{}; //!< An atomically-incrementing counter at the core of a syncpoint

        std::mutex mutex; //!< Synchronizes insertions and deletions of waiters alongside locking the increment condition
        std::condition_variable incrementCondition; //!< Signalled on thresholds for waiters which are tied to Wait(...)

        struct Waiter {
            u32 threshold; //!< The syncpoint value to wait on to be reached
            std::function<void()> callback; //!< The callback to do after the wait has ended, refers to cvar signal when nullptr

            Waiter(u32 threshold, std::function<void()> callback) : threshold(threshold), callback(std::move(callback)) {}
        };
        std::list<Waiter> waiters; //!< A linked list of all waiters, it's sorted in ascending order by threshold

      public:
        /**
         * @return The value of the syncpoint, retrieved in an atomically safe manner
         */
        constexpr u32 Load() {
            return value.load(std::memory_order_acquire);
        }

        using WaiterHandle = decltype(waiters)::iterator; //!< Aliasing an iterator to a Waiter as an opaque handle

        /**
         * @brief Registers a new waiter with a callback that will be called when the syncpoint reaches the target threshold
         * @note The callback will be called immediately if the syncpoint has already reached the given threshold
         * @return A handle that can be used to deregister the waiter, its boolean operator will evaluate to false if the threshold has already been reached
         */
        WaiterHandle RegisterWaiter(u32 threshold, const std::function<void()> &callback);

        /**
         * @note If the supplied handle is invalid then the function will do nothing
         */
        void DeregisterWaiter(WaiterHandle waiter);

        /**
         * @return The new value of the syncpoint after the increment
         */
        u32 Increment();

        /**
         * @brief Waits for the syncpoint to reach given threshold
         * @return If the wait was successful (true) or timed out (false)
         * @note Guaranteed to succeed when 'steady_clock::duration::max()' is used
         */
        bool Wait(u32 threshold, std::chrono::steady_clock::duration timeout);
    };

    /**
     * @bried Holds host and guest copies of an individual syncpoint
     */
    struct SyncpointPair {
        Syncpoint guest; //!< Incremented at GPFIFO processing time
        Syncpoint host; //!< Incremented after host GPU completion

        void Increment() {
            guest.Increment();
            host.Increment();
        }
    };

    using SyncpointSet = std::array<SyncpointPair, SyncpointCount>;
}
