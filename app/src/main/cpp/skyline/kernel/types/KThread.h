// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

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
        constexpr i8 DefaultPriority{44}; // The default priority of an HOS process
        constexpr kernel::type::Priority AndroidPriority{19, -8}; //!< The range of priorities for Android
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

            void StartThread();

          public:
            std::atomic<bool> running{false};
            std::atomic<bool> cancelSync{false}; //!< This is to flag to a thread to cancel a synchronization call it currently is in

            KHandle handle;
            size_t id; //!< Index of thread in parent process's KThread vector

            std::shared_ptr<KPrivateMemory> stack;
            nce::ThreadContext ctx{};

            void* entry;
            u64 entryArgument;
            i8 priority;

            KThread(const DeviceState &state, KHandle handle, KProcess *parent, size_t id, void *entry, u64 argument = 0, i8 priority = constant::DefaultPriority, const std::shared_ptr<KPrivateMemory>& stack = nullptr);

            ~KThread();

            /**
             * @param self If the calling thread should jump directly into guest code or if a new thread should be created for it
             * @note If the thread is already running then this does nothing
             * @note 'stack' will be created if it wasn't set prior to calling this
             */
            void Start(bool self = false);

            void Kill();

            /**
             * @brief Sets the host priority using setpriority with a rescaled the priority from HOS to Android
             * @note It also affects guest scheduler behavior, this isn't purely for host
             */
            void UpdatePriority(i8 priority);
        };
    }
}
