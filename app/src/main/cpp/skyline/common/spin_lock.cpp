// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <chrono>
#include <thread>
#include "spin_lock.h"
#include "utils.h"

namespace skyline {
    static constexpr size_t LockAttemptsPerYield{32};
    static constexpr size_t LockAttemptsPerSleep{1024};
    static constexpr size_t SleepDurationUs{50};

    template<typename Func>
    void FalloffLock(Func &&func) {
        for (size_t i{1}; !func(i); i++) {
            asm volatile("DMB ISHST;"
                         "YIELD;");

            if (i % LockAttemptsPerYield == 0)
                std::this_thread::yield();
            if (i % LockAttemptsPerSleep == 0)
                std::this_thread::sleep_for(std::chrono::microseconds(SleepDurationUs));
        }
    }

    void  __attribute__ ((noinline)) SpinLock::LockSlow() {
        FalloffLock([this] (size_t i) {
            return try_lock();
        });
    }

    void  __attribute__ ((noinline)) SharedSpinLock::LockSlow() {
        FalloffLock([this] (size_t i) {
            return try_lock();
        });
    }

    void  __attribute__ ((noinline)) SharedSpinLock::LockSlowShared() {
        FalloffLock([this] (size_t i) {
            return try_lock_shared();
        });
    }

    static constexpr size_t AdaptiveWaitIters{1024}; //!< Number of wait iterations before waiting should fallback to a regular condition variable

    void __attribute__ ((noinline)) AdaptiveSingleWaiterConditionVariable::SpinWait() {
        FalloffLock([this] (size_t i) {
            return !unsignalled.test_and_set() || i >= AdaptiveWaitIters;
        });
    }

    void __attribute__ ((noinline)) AdaptiveSingleWaiterConditionVariable::SpinWait(i64 maxEndTimeNs) {
        FalloffLock([maxEndTimeNs, this] (size_t i) {
            return util::GetTimeNs() > maxEndTimeNs  || !unsignalled.test_and_set() || i >= AdaptiveWaitIters;
        });
    }
}
