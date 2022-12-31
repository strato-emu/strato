// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <chrono>
#include <thread>
#include "spin_lock.h"

namespace skyline {
    static constexpr size_t LockAttemptsPerYield{256};
    static constexpr size_t LockAttemptsPerSleep{1024};
    static constexpr size_t SleepDurationUs{1000};

    void  __attribute__ ((noinline)) SpinLock::LockSlow() {
        // We need to start with attempt = 1, otherwise
        // attempt % LockAttemptsPerSleep is zero for the first iteration.
        size_t attempt{1};
        while (true) {
            if (!locked.test_and_set(std::memory_order_acquire))
                return;

            attempt++;
            if (attempt % LockAttemptsPerSleep == 0)
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            else if (attempt % LockAttemptsPerYield == 0)
                std::this_thread::yield();
        }
    }

}
