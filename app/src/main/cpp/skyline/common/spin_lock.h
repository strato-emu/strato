// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <atomic>
#include <condition_variable>
#include <thread>
#include <mutex>
#include "base.h"
#include "utils.h"

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

    /**
     * @brief A condition variable that spins for a bit before falling back to a regular condition variable, for cases where only a single thread can wait at the same time
     */
    class AdaptiveSingleWaiterConditionVariable {
      private:
        /**
         * @brief Spins either until the condition variable is signalled or the spin wait times out (to fall back to a regular condition variable)
         */
        void SpinWait();

        /**
         * @brief Spins either until the condition variable is signalled or the spin wait times out (to fall back to a regular condition variable, or until the given time is reached)
         * @param maxEndTimeNs The maximum time to spin for
         */
        void SpinWait(i64 maxEndTimeNs);

        std::condition_variable fallback; //<! Fallback condition variable for when the spin wait times out
        std::mutex fallbackMutex; //!< Used to allow taking in arbitrary mutex types and to synchronise access to fallbackWaiter
        std::atomic_flag unsignalled{true}; //!< Set to false when the condition variable is signalled
        bool fallbackWaiter{}; //!< True if the waiter is waiting on the fallback condition variable

      public:
        /**
         * @brief Signals the condition variable
         */
        void notify() {
            unsignalled.clear();

            std::scoped_lock fallbackLock{fallbackMutex};
            if (fallbackWaiter)
                fallback.notify_one();
        }

        /**
         * @brief Waits for the condition variable to be signalled
         * @param lock The lock to unlock while waiting
         * @param pred The predicate to check, if it returns true then the wait will end
         */
        void wait(auto &lock, auto pred) {
            // 'notify' calls should only wake the condition variable when called during waiting
            unsignalled.test_and_set();

            if (!pred()) {
                // First spin wait for a bit, to hopefully avoid the costs of condition variables under heavy thrashing
                lock.unlock();
                SpinWait();
                lock.lock();
            } else {
                return;
            }

            // The spin wait has either timed out or succeeded, check the predicate to confirm which is the case
            if (pred())
                return;

            // If the spin wait timed out then fallback to a regular condition variable
            while (!pred()) {
                std::unique_lock fallbackLock{fallbackMutex};

                // Store that we are currently waiting on the fallback condition variable, `notify()` can check this when `fallbackLock` ends up being unlocked by `fallback.wait()` to avoid redundantly performing futex wakes (on older bionic versions)
                fallbackWaiter = true;

                lock.unlock();
                fallback.wait(fallbackLock);

                // The predicate has been satisfied, we're done here
                fallbackWaiter = false;
                fallbackMutex.unlock();

                lock.lock();
            }
        }

        /**
         * @brief Waits for the condition variable to be signalled or the given duration to elapse
         * @param lock The lock to unlock while waiting
         * @param duration The duration to wait for
         * @param pred The predicate to check, if it returns true then the wait will end
         * @return True if the predicate returned true, false if the duration elapsed
         */
        bool wait_for(auto &lock, const auto &duration, auto pred) {
            // 'notify' calls should only wake the condition variable when called during waiting
            unsignalled.test_and_set();

            auto endTimeNs{util::GetTimeNs() + std::chrono::nanoseconds(duration).count()};

            if (!pred()) {
                // First spin wait for a bit, to hopefully avoid the costs of condition variables under heavy thrashing
                lock.unlock();
                SpinWait(endTimeNs);
                lock.lock();
            } else {
                return true;
            }

            // The spin wait has either timed out (due to wanting to fallback), timed out (due to the duration being exceeded) or succeeded, check the predicate and current time to confirm which is the case
            if (pred())
                return true;
            else if (util::GetTimeNs() > endTimeNs)
                return false;


            // If the spin wait timed out (due to wanting to fallback), then fallback to a regular condition variable

            // Calculate chrono-based end time only in the fallback path to avoid polluting the fast past
            auto endTime{std::chrono::system_clock::now() + std::chrono::nanoseconds(endTimeNs - util::GetTimeNs())};
            std::cv_status status{std::cv_status::no_timeout};
            while (status == std::cv_status::no_timeout && !pred()) {
                std::unique_lock fallbackLock{fallbackMutex};

                // Store that we are currently waiting on the fallback condition variable, `notify()` can check this when `fallbackLock` ends up being unlocked by `fallback.wait()` to avoid redundantly performing futex wakes (on older bionic versions)
                fallbackWaiter = true;

                lock.unlock();
                status = fallback.wait_until(fallbackLock, endTime);

                fallbackWaiter = false;
                fallbackLock.unlock();

                lock.lock();
            }

            // The predicate has been satisfied, we're done here
            return pred();
        }
    };
}
