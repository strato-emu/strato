// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <csetjmp>
#include <nce/guest.h>
#include "KSyncObject.h"
#include "KPrivateMemory.h"
#include "KSharedMemory.h"

namespace skyline {
    namespace kernel::type {
        struct Priority {
            i8 low; //!< The low range of priority
            i8 high; //!< The high range of priority

            /**
             * @param priority The priority range of the value
             * @param value The priority value to rescale
             * @return The rescaled priority value according to this range
             */
            constexpr i8 Rescale(const Priority &priority, i8 value) const {
                return static_cast<i8>(priority.low + ((static_cast<float>(priority.high - priority.low) / static_cast<float>(priority.low - priority.high)) * (static_cast<float>(value) - priority.low)));
            }

            /**
             * @param value The priority value to check for validity
             * @return If the supplied priority value is valid
             */
            constexpr bool Valid(i8 value) const {
                return (value >= low) && (value <= high);
            }
        };
    }

    namespace constant {
        constexpr u8 CoreCount{4}; // The amount of cores an HOS process can be scheduled onto (User applications can only be on the first 3 cores, the last one is reserved for the system)
        constexpr kernel::type::Priority HosPriority{0, 63}; //!< The range of priorities for Horizon OS
    }

    namespace kernel::type {
        /**
         * @brief KThread manages a single thread of execution which is responsible for running guest code and kernel code which is invoked by the guest
         */
        class KThread : public KSyncObject, public std::enable_shared_from_this<KThread> {
          private:
            KProcess *parent;
            std::optional<std::thread> thread; //!< If this KThread is backed by a host thread then this'll hold it
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
            i8 idealCore;
            i8 coreId; //!< The CPU core on which this thread is running
            std::bitset<constant::CoreCount> affinityMask{}; //!< The CPU core on which this thread is running

            KThread(const DeviceState &state, KHandle handle, KProcess *parent, size_t id, void *entry, u64 argument, void *stackTop, i8 priority, i8 idealCore);

            ~KThread();

            /**
             * @param self If the calling thread should jump directly into guest code or if a new thread should be created for it
             * @note If the thread is already running then this does nothing
             * @note 'stack' will be created if it wasn't set prior to calling this
             */
            void Start(bool self = false);

            /**
             * @param join Returns after the guest thread has joined rather than instantly
             */
            void Kill(bool join);

            void UpdatePriority(i8 priority);
        };
    }
}
