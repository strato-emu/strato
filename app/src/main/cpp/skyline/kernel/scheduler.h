// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <condition_variable>

namespace skyline {
    namespace constant {
        constexpr u8 CoreCount{4}; //!< The amount of cores an HOS process can be scheduled onto (User applications can only be on the first 3 cores, the last one is reserved for the system)
        constexpr u8 ParkedCoreId{CoreCount}; //!< // An invalid core ID, representing that a thread has been parked
    }

    namespace kernel {
        using CoreMask = std::bitset<constant::CoreCount>;

        /**
         * @brief Priority on HOS determines scheduling behavior relative to other threads
         * @note Lower priority values result in a higher priority, similar to niceness on Linux
         */
        struct Priority {
            u8 min; //!< Numerically lowest priority, highest scheduler priority
            u8 max; //!< Numerically highest priority, lowest scheduler priority

            /**
             * @return A bitmask with each bit corresponding to if scheduler priority with the same index is valid
             */
            constexpr u64 Mask() const {
                return (std::numeric_limits<u64>::max() >> ((std::numeric_limits<u64>::digits - 1 + min) - max)) << min;
            }

            constexpr bool Valid(u8 value) const {
                return (value >= min) && (value <= max);
            }
        };

        /*
         * @brief The Scheduler is responsible for determining which threads should run on which virtual cores and when they should be scheduled
         * @note We tend to stray a lot from HOS in our scheduler design as we've designed it around our 1 host thread per guest thread which leads to scheduling from the perspective of threads while the HOS scheduler deals with scheduling from the perspective of cores, not doing this would lead to missing out on key optimizations and serialization of scheduling
         */
        class Scheduler {
          private:
            const DeviceState &state;

            struct CoreContext {
                u8 id;
                u8 preemptionPriority; //!< The priority at which this core becomes preemptive as opposed to cooperative
                std::mutex mutex; //!< Synchronizes all operations on the queue
                std::list<std::shared_ptr<type::KThread>> queue; //!< A queue of threads which are running or to be run on this core

                CoreContext(u8 id, u8 preemptionPriority);
            };

            std::array<CoreContext, constant::CoreCount> cores{CoreContext(0, 59), CoreContext(1, 59), CoreContext(2, 59), CoreContext(3, 63)};

            std::mutex parkedMutex; //!< Synchronizes all operations on the queue of parked threads
            std::condition_variable parkedFrontCondition; //!< A conditional variable which is signalled when the front of the parked queue has changed
            std::list<std::shared_ptr<type::KThread>> parkedQueue; //!< A queue of threads which are parked and waiting on core migration

          public:
            static constexpr std::chrono::milliseconds PreemptiveTimeslice{10}; //!< The duration of time a preemptive thread can run before yielding
            inline static int YieldSignal{SIGRTMIN}; //!< The signal used to cause a yield in running threads
            inline static thread_local bool YieldPending{}; //!< A flag denoting if a yield is pending on this thread, it's checked at SVC exit

            Scheduler(const DeviceState &state);

            /**
             * @brief A signal handler designed to cause a non-cooperative yield for preemption and higher priority threads being inserted
             */
            static void SignalHandler(int signal, siginfo *info, ucontext *ctx, void **tls);

            /**
             * @brief Checks all cores and migrates the specified thread to the core where the calling thread should be scheduled the earliest
             * @param alwaysInsert If to insert the thread even if it hasn't migrated cores, this is used during thread creation
             * @return A reference to the CoreContext of the core which the calling thread is running on after load balancing
             * @note This inserts the thread into the migrated process's queue after load balancing, there is no need to call it redundantly
             * @note alwaysInsert makes the assumption that the thread isn't inserted in any core's queue currently
             */
            CoreContext& LoadBalance(const std::shared_ptr<type::KThread> &thread, bool alwaysInsert = false);

            /**
             * @brief Inserts the specified thread into the scheduler queue at the appropriate location based on it's priority
             */
            void InsertThread(const std::shared_ptr<type::KThread>& thread);

            /**
             * @brief Wait for the current thread to be scheduled on it's resident core
             * @param loadBalance If the thread is appropriate for load balancing then if to load balance it occassionally or not
             * @note There is an assumption of the thread being on it's resident core queue, if it's not this'll never return
             */
            void WaitSchedule(bool loadBalance = true);

            /**
             * @brief Wait for the current thread to be scheduled on it's resident core or for the timeout to expire
             * @return If the thread has been scheduled (true) or if the timer expired before it could be (false)
             * @note This will never load balance as it uses the timeout itself as a result this shouldn't be used as a replacement for regular waits
             */
            bool TimedWaitSchedule(std::chrono::nanoseconds timeout);

            /**
             * @brief Rotates the calling thread's resident core queue, if it is at the front of it
             * @param cooperative If this was triggered by a cooperative yield as opposed to a preemptive one
             */
            void Rotate(bool cooperative = true);

            /**
             * @brief Updates the placement of the supplied thread in it's resident core's queue according to it's new priority
             */
            void UpdatePriority(const std::shared_ptr<type::KThread>& thread);

            /**
             * @brief Parks the calling thread after removing it from it's resident core's queue and inserts it on the core it's been awoken on
             * @note This will not handle waiting for the thread to be scheduled, this should be followed with a call to WaitSchedule/TimedWaitSchedule
             */
            void ParkThread();

            /**
             * @brief Wakes a single parked thread which may be appropriate for running next on this core
             * @note We will only wake a thread if it is determined to be a better pick than the thread which would be run on this core next
             */
            void WakeParkedThread();

            /**
             * @brief Removes the calling thread from it's resident core queue
             */
            void RemoveThread();
        };

        /**
         * @brief A lock which removes the calling thread from it's resident core's scheduler queue and adds it back when being destroyed
         * @note It also blocks till the thread has been rescheduled in it's destructor, this behavior might not be preferable in some cases
         * @note This is not an analogue to KScopedSchedulerLock on HOS, it is for handling thread state changes which we handle with Scheduler::YieldPending
         */
        struct SchedulerScopedLock {
          private:
            const DeviceState& state;

          public:
            inline SchedulerScopedLock(const DeviceState& state) : state(state) {
                state.scheduler->RemoveThread();
            }

            inline ~SchedulerScopedLock() {
                state.scheduler->InsertThread(state.thread);
                state.scheduler->WaitSchedule();
            }
        };
    }
}
