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
            if (try_lock()) [[likely]]
                return;

            LockSlow();
        }

        bool try_lock() {
            return !locked.test_and_set(std::memory_order_acq_rel);
        }

        void unlock() {
            locked.clear(std::memory_order_release);
        }
    };

    /**
     * @brief Spinlock variant of std::shared_mutex
     * @note This is loosely based on https://github.com/facebook/folly/blob/224350ea8c7c183312bec653e0d95a2b1e356ed7/folly/synchronization/RWSpinLock.h
     */
    class SharedSpinLock {
      private:
        static constexpr u32 StateReader{2};
        static constexpr u32 StateWriter{1};

        std::atomic<u32> state{};

        void LockSlow();

        void LockSlowShared();

      public:
        void lock() {
            if (try_lock()) [[likely]]
                return;

            LockSlow();
        }

        void lock_shared() {
            if (try_lock_shared()) [[likely]]
                return;

            LockSlowShared();
        }

        bool try_lock() {
            u32 expected{};
            return state.compare_exchange_strong(expected, StateWriter, std::memory_order_acq_rel);
        }

        bool try_lock_shared() {
            u32 value{state.fetch_add(StateReader, std::memory_order_acquire)};
            if (value & StateWriter) {
                state.fetch_add(-StateReader, std::memory_order_release);
                return false;
            }
            return true;
        }

        void unlock() {
            state.fetch_and(~StateWriter, std::memory_order_release);
        }

        void unlock_shared() {
            state.fetch_add(-StateReader, std::memory_order_release);
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
