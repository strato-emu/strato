// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <csetjmp>
#include <nce/guest.h>
#include <kernel/scheduler.h>
#include "KSyncObject.h"
#include "KPrivateMemory.h"
#include "KSharedMemory.h"

namespace skyline {
    namespace kernel::type {
        /**
         * @brief KThread manages a single thread of execution which is responsible for running guest code and kernel code which is invoked by the guest
         */
        class KThread : public KSyncObject, public std::enable_shared_from_this<KThread> {
          private:
            KProcess *parent;
            std::thread thread; //!< If this KThread is backed by a host thread then this'll hold it
            pthread_t pthread{}; //!< The pthread_t for the host thread running this guest thread

            void StartThread();

          public:
            std::mutex mutex; //!< Synchronizes all thread state changes
            bool running{false};
            std::atomic<bool> cancelSync{false}; //!< This is to flag to a thread to cancel a synchronization call it currently is in

            KHandle handle;
            size_t id; //!< Index of thread in parent process's KThread vector

            nce::ThreadContext ctx{}; //!< The context of the guest thread during the last SVC
            jmp_buf originalCtx; //!< The context of the host thread prior to jumping into guest code

            void *entry;
            u64 entryArgument;
            void *stackTop;

            i8 priority;
            i8 idealCore; //!< The ideal CPU core for this thread to run on
            i8 coreId; //!< The CPU core on which this thread is running
            CoreMask affinityMask{}; //!< A mask of CPU cores this thread is allowed to run on
            u64 timesliceStart{}; //!< Start of the scheduler timeslice
            u64 averageTimeslice{}; //!< A weighted average of the timeslice duration for this thread
            std::optional<timer_t> preemptionTimer{}; //!< A kernel timer used for preemption interrupts
            bool isPreempted{}; //!< If the preemption timer has been armed and will fire

            KThread(const DeviceState &state, KHandle handle, KProcess *parent, size_t id, void *entry, u64 argument, void *stackTop, i8 priority, i8 idealCore);

            ~KThread();

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

            void UpdatePriority(i8 priority);
        };
    }
}
