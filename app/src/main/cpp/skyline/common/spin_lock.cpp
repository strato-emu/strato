// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <chrono>
#include <thread>
#include "spin_lock.h"

namespace skyline {
    static constexpr size_t LockAttemptsPerYield{256};
    static constexpr size_t LockAttemptsPerSleep{1024};
    static constexpr size_t SleepDurationUs{100};

    template<typename Func>
    void FalloffLock(Func &&func) {
        for (size_t i{}; !func(); i++) {
            if (i % LockAttemptsPerYield == 0)
                std::this_thread::yield();
            if (i % LockAttemptsPerSleep == 0)
                std::this_thread::sleep_for(std::chrono::microseconds(SleepDurationUs));
        }
    }

    void  __attribute__ ((noinline)) SpinLock::LockSlow() {
        FalloffLock([this] {
            return try_lock();
        });
    }

    void  __attribute__ ((noinline)) SharedSpinLock::LockSlow() {
        FalloffLock([this] {
            return try_lock();
        });
    }

    void  __attribute__ ((noinline)) SharedSpinLock::LockSlowShared() {
        FalloffLock([this] {
            return try_lock_shared();
        });
    }
}
