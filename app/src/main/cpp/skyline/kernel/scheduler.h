// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common/spin_lock.h"
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
            i8 min; //!< Numerically lowest priority, highest scheduler priority
            i8 max; //!< Numerically highest priority, lowest scheduler priority

            /**
             * @return A bitmask with each bit corresponding to if scheduler priority with the same index is valid
             */
            constexpr u64 Mask() const {
                return (std::numeric_limits<u64>::max() >> ((std::numeric_limits<u64>::digits - 1 + min) - max)) << min;
            }

            constexpr bool Valid(i8 value) const {
                return (value >= min) && (value <= max);
            }
        };

        /**
         * @brief The Scheduler is responsible for determining which threads should run on which virtual cores and when they should be scheduled
         * @note We tend to stray a lot from HOS in our scheduler design as we've designed it around our 1 host thread per guest thread which leads to scheduling from the perspective of threads while the HOS scheduler deals with scheduling from the perspective of cores, not doing this would lead to missing out on key optimizations and serialization of scheduling
         */
        class Scheduler {
          private:
            const DeviceState &state;

            struct CoreContext {
                u8 id;
                i8 preemptionPriority; //!< The priority at which this core becomes preemptive as opposed to cooperative
                SpinLock mutex; //!< Synchronizes all operations on the queue
                std::list<std::shared_ptr<type::KThread>> queue; //!< A queue of threads which are running or to be run on this core

                CoreContext(u8 id, i8 preemptionPriority);
            };

            std::array<CoreContext, constant::CoreCount> cores{CoreContext(0, 59), CoreContext(1, 59), CoreContext(2, 59), CoreContext(3, 63)};

            std::mutex parkedMutex; //!< Synchronizes all operations on the queue of parked threads
            std::list<std::shared_ptr<type::KThread>> parkedQueue; //!< A queue of threads which are parked and waiting on core migration

            /**
             * @brief Migrate a thread from its resident core to the target core
             * @note 'KThread::coreMigrationMutex' **must** be locked by the calling thread prior to calling this
             * @note This is used to handle non-cooperative core affinity mask changes where the resident core is not in its new affinity mask
             */
            void MigrateToCore(const std::shared_ptr<type::KThread> &thread, CoreContext *&currentCore, CoreContext *targetCore, std::unique_lock<SpinLock> &lock);

            /**
             * @brief Trigger a thread to yield via a signal or on SVC exit if it is the current thread
             */
            void YieldThread(const std::shared_ptr<type::KThread> &thread);

          public:
            static constexpr std::chrono::milliseconds PreemptiveTimeslice{10}; //!< The duration of time a preemptive thread can run before yielding
            inline static int YieldSignal{SIGRTMIN}; //!< The signal used to cause a non-cooperative yield in running threads
            inline static int PreemptionSignal{SIGRTMIN + 1}; //!< The signal used to cause a preemptive yield in running threads
            inline static thread_local bool YieldPending{}; //!< A flag denoting if a yield is pending on this thread, it's checked prior to entering guest code as signals cannot interrupt host code

            Scheduler(const DeviceState &state);

            /**
             * @brief A signal handler designed to cause a non-cooperative yield for preemption and higher priority threads being inserted
             */
            static void SignalHandler(int signal, siginfo *info, ucontext *ctx, void **tls);

            /**
             * @brief Checks all cores and determines the core where the supplied thread should be scheduled the earliest
             * @note 'KThread::coreMigrationMutex' **must** be locked by the calling thread prior to calling this
             * @note No core mutexes should be held by the calling thread, that will cause a recursive lock and lead to a deadlock
             * @return A reference to the CoreContext of the optimal core
             */
            CoreContext &GetOptimalCoreForThread(const std::shared_ptr<type::KThread> &thread);

            /**
             * @brief Inserts the specified thread into the scheduler queue at the appropriate location based on its priority
             * @note This is a non-blocking operation when the thread is paused, the thread will only be inserted when it is resumed
             */
            void InsertThread(const std::shared_ptr<type::KThread> &thread);

            /**
             * @brief Wait for the calling thread to be scheduled on its resident core
             * @param loadBalance If the thread is appropriate for load balancing then if to load balance it occassionally or not
             * @note There is an assumption of the thread being on its resident core queue, if it's not this'll never return
             */
            void WaitSchedule(bool loadBalance = true);

            /**
             * @brief Wait for the calling thread to be scheduled on its resident core or for the timeout to expire
             * @return If the thread has been scheduled (true) or if the timer expired before it could be (false)
             * @note This will never load balance as it uses the timeout itself as a result this shouldn't be used as a replacement for regular waits
             */
            bool TimedWaitSchedule(std::chrono::nanoseconds timeout);

            /**
             * @brief Rotates the calling thread's resident core queue, if it's at the front of it
             * @param cooperative If this was triggered by a cooperative yield as opposed to a preemptive one
             */
            void Rotate(bool cooperative = true);

            /**
             * @brief Removes the calling thread from its resident core queue
             */
            void RemoveThread();

            /**
             * @brief Updates the placement of the supplied thread in its resident core's queue according to its current priority
             */
            void UpdatePriority(const std::shared_ptr<type::KThread> &thread);

            /**
             * @brief Updates the core that the supplied thread is resident to according to its new affinity mask and ideal core
             * @note This supports changing the core of a thread which is currently running
             * @note 'KThread::coreMigrationMutex' **must** be locked by the calling thread prior to calling this
             */
            void UpdateCore(const std::shared_ptr<type::KThread> &thread);

            /**
             * @brief Parks the calling thread after removing it from its resident core's queue and inserts it on the core it's been awoken on
             * @note This will not handle waiting for the thread to be scheduled, this should be followed with a call to WaitSchedule/TimedWaitSchedule
             */
            void ParkThread();

            /**
             * @brief Wakes a single parked thread which may be appropriate for running next on this core
             * @note We will only wake a thread if it's determined to be a better pick than the thread which would be run on this core next
             */
            void WakeParkedThread();

            /**
             * @brief Pauses the supplied thread till a corresponding call to ResumeThread has been made
             * @note 'KThread::coreMigrationMutex' **must** be locked by the calling thread prior to calling this
             */
            void PauseThread(const std::shared_ptr<type::KThread> &thread);

            /**
             * @brief Resumes a thread which was previously paused by a call to PauseThread
             * @note 'KThread::coreMigrationMutex' **must** be locked by the calling thread prior to calling this
             */
            void ResumeThread(const std::shared_ptr<type::KThread> &thread);
        };

        /**
         * @brief A lock which removes the calling thread from its resident core's scheduler queue and adds it back when being destroyed
         * @note It also blocks till the thread has been rescheduled in its destructor, this behavior might not be preferable in some cases
         * @note This is not an analogue to KScopedSchedulerLock on HOS, it's for handling thread state changes which we handle with Scheduler::YieldPending
         */
        struct SchedulerScopedLock {
          private:
            const DeviceState &state;

          public:
            SchedulerScopedLock(const DeviceState &state) : state(state) {
                state.scheduler->RemoveThread();
            }

            ~SchedulerScopedLock() {
                state.scheduler->InsertThread(state.thread);
                state.scheduler->WaitSchedule();
            }
        };
    }
}
