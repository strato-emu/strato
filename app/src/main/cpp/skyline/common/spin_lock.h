// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <atomic>
#include <thread>
#include "base.h"

namespace skyline {
    /**
     * @brief A simple spin lock implementation with a yield and sleep fallback
     * @note This should *ONLY* be used in situations where it is provably better than an std::mutex due to spinlocks having worse perfomance under heavy contention
     */
    class SpinLock {
      private:
        std::atomic_flag locked{};

        void LockSlow();

      public:
        void lock() {
            if (!locked.test_and_set(std::memory_order_acquire)) [[likely]]
                return;

            LockSlow();
        }

        bool try_lock() {
            return !locked.test_and_set(std::memory_order_acquire);
        }

        void unlock() {
            locked.clear(std::memory_order_release);
        }
    };

    /**
     * @brief Recursive lock built ontop of `SpinLock`
     * @note This should *ONLY* be used in situations where it is provably better than an std::mutex due to spinlocks having worse perfomance under heavy contention
     */
    class RecursiveSpinLock {
      private:
        SpinLock backingLock;
        u32 uses{};
        std::thread::id tid{};

      public:
        void lock() {
            if (tid == std::this_thread::get_id()) {
                uses++;
            } else {
                backingLock.lock();
                tid = std::this_thread::get_id();
                uses = 1;
            }
        }

        bool try_lock() {
            if (tid == std::this_thread::get_id()) {
                uses++;
                return true;
            } else {
                if (backingLock.try_lock()) {
                    tid = std::this_thread::get_id();
                    uses = 1;
                    return true;
                }
                return false;
            }
        }

        void unlock() {
            if (--uses == 0) {
                tid = {};
                backingLock.unlock();
            }
        }
    };
}
