// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <csetjmp>
#include <nce/guest.h>
#include <jit/thread_context32.h>
#include <kernel/scheduler.h>
#include <common/signal.h>
#include <common/spin_lock.h>
#include "KSyncObject.h"
#include "KSharedMemory.h"

namespace skyline::kernel {
    thread_local inline type::KThread *this_thread{nullptr}; //!< The guest KThread for the current host thread

    namespace type {
        /**
         * @brief KThread manages a single thread of execution which is responsible for running guest code and kernel code which is invoked by the guest
         */
        class KThread : public KSyncObject, public std::enable_shared_from_this<KThread> {
          public:
            KProcess &process; //!< The process this thread belongs to

          private:
            std::thread thread; //!< If this KThread is backed by a host thread then this'll hold it
            pthread_t pthread{}; //!< The pthread_t for the host thread running this guest thread
            timer_t preemptionTimer{}; //!< A kernel timer used for preemption interrupts

            /**
             * @brief Entry function of any guest threads, sets up necessary context and jumps into guest code from the calling thread
             */
            void ThreadEntrypoint();

          protected:
            /**
             * @brief Initializes the thread's execution-mode-specific host object
             */
            virtual void Init() = 0;

            /**
             * @brief Runs this thread's guest code
             */
            virtual void Run() = 0;

          public:
            std::mutex statusMutex; //!< Synchronizes all thread state changes (running/ready/killed)
            std::condition_variable statusCondition; //!< Signalled on the status of the thread changing
            bool running{false}; //!< If the host thread that corresponds to this thread is running, this doesn't reflect guest scheduling changes
            bool ready{false}; //!< If this thread is ready to receive signals or not
            bool killed{false}; //!< If this thread was previously running and has been killed

            KHandle handle;
            size_t id; //!< Index of thread in parent process's KThread vector

            jmp_buf originalCtx{}; //!< The context of the host thread prior to jumping into guest code

            void *entry; //!< A function pointer to the thread's entry
            u64 entryArgument; //!< An argument to provide with to the thread entry function
            void *stackTop; //!< The top of the guest's stack, this is set to the initial guest stack pointer
            u8 *tlsRegion{}; //!< The TLS region for this thread, this is a host pointer so it may need translation before use in guest code

            AdaptiveSingleWaiterConditionVariable scheduleCondition; //!< Signalled to wake the thread when it's scheduled or its resident core changes
            std::atomic<i8> basePriority; //!< The priority of the thread for the scheduler without any priority-inheritance
            std::atomic<i8> priority; //!< The priority of the thread for the scheduler including priority-inheritance

            std::recursive_mutex coreMigrationMutex; //!< Synchronizes operations which depend on which core the thread is running on
            u8 idealCore; //!< The ideal CPU core for this thread to run on
            u8 coreId; //!< The CPU core on which this thread is running
            CoreMask affinityMask{}; //!< A mask of CPU cores this thread is allowed to run on

            u64 timesliceStart{}; //!< A timestamp in host CNTVCT ticks of when the thread's current timeslice started
            u64 averageTimeslice{}; //!< A weighted average of the timeslice duration for this thread

            bool isPreempted{}; //!< If the preemption timer has been armed and will fire
            bool pendingYield{}; //!< If the thread has been yielded and hasn't been acted upon it yet
            bool forceYield{}; //!< If the thread has been forcefully yielded by another thread

            RecursiveSpinLock waiterMutex; //!< Synchronizes operations on mutation of the waiter members
            u32 *waitMutex{}; //!< The key of the mutex which this thread is waiting on
            KHandle waitTag{}; //!< The handle of the thread which requested the mutex lock
            std::shared_ptr<KThread> waitThread; //!< The thread which this thread is waiting on
            std::list<std::shared_ptr<type::KThread>> waiters; //!< A queue of threads waiting on this thread sorted by priority
            void *waitConditionVariable{}; //!< The condition variable which this thread is waiting on
            bool waitSignalled{}; //!< If the conditional variable has been signalled already
            Result waitResult; //!< The result of the wait operation

            bool isCancellable{false}; //!< If the thread is currently in a position where it's cancellable
            bool cancelSync{false}; //!< Whether to cancel the SvcWaitSynchronization call this thread currently is in/the next one it joins
            type::KSyncObject *wakeObject{}; //!< A pointer to the synchronization object responsible for waking this thread up

            bool isPaused{false}; //!< If the thread is currently paused and not runnable
            bool insertThreadOnResume{false}; //!< If the thread should be inserted into the scheduler when it resumes (used for pausing threads during sleep/sync)

            jit::JitCore32 *jit{nullptr}; //!< The JIT core this thread is running on, or nullptr if it's not currently running

            KThread(const DeviceState &state, KHandle handle, KProcess &process, size_t id, void *entry, u64 argument, void *stackTop, i8 priority, u8 idealCore);

            ~KThread() override;

            /**
             * @param self If the calling thread should jump directly into guest code or if a new thread should be created for it
             * @note If the thread is already running then this does nothing
             * @note 'stack' will be created if it wasn't set prior to calling this
             */
            void Start(bool self = false);

            /**
             * @param join Return after the thread has joined rather than instantly
             */
            void Kill(bool join);

            /**
             * @brief Sends a host OS signal to the thread which is running this KThread
             */
            void SendSignal(int signal);

            /**
             * @brief Arms the preemption kernel timer to fire in the specified amount of time
             */
            void ArmPreemptionTimer(std::chrono::nanoseconds timeToFire);

            /**
             * @brief Disarms the preemption kernel timer, any scheduled firings will be cancelled
             */
            void DisarmPreemptionTimer();

            /**
             * @brief Recursively updates the priority for any threads this thread might be waiting on
             * @note PI is performed by temporarily upgrading a thread's priority if a thread waiting on it has a higher priority to prevent priority inversion
             * @note This will lock `waiterMutex` internally and it must **not** be held when calling this function
             */
            void UpdatePriorityInheritance();

            /**
             * @return If the supplied priority value is higher than the supplied thread's priority value
             */
            static constexpr bool IsHigherPriority(const i8 priority, const std::shared_ptr<type::KThread> &it) {
                return priority < it->priority;
            }
        };

        class KNceThread : public KThread {
            /**
             * @brief Initializes the thread object for execution in NCE mode
             */
            void Init() override;

            /**
             * @brief Entry function for any guest thread when running in NCE mode
             * @note This function does not return as it jumps into guest code
             */
            void Run() override;

          public:
            nce::ThreadContext ctx{}; //!< The context of the guest thread during the last SVC

            using KThread::KThread;
        };

        class KJit32Thread : public KThread {
            /**
             * @brief Initializes the thread object for execution in 32-bit JIT mode
             */
            void Init() override;

            /**
             * @brief Entry function for any guest thread when running in 32-bit JIT mode
             */
            void Run() override;

          public:
            jit::ThreadContext32 ctx{}; //!< The context of the guest thread

            using KThread::KThread;
        };
    }
}
