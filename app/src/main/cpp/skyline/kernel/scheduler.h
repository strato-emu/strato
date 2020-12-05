// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <condition_variable>

namespace skyline {
    namespace constant {
        constexpr u8 CoreCount{4}; // The amount of cores an HOS process can be scheduled onto (User applications can only be on the first 3 cores, the last one is reserved for the system)
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

            constexpr bool Valid(i8 value) const {
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
                std::shared_mutex mutex; //!< Synchronizes all operations on the queue
                std::condition_variable_any mutateCondition; //!< A conditional variable which is signalled on every mutation of the queue
                std::deque<std::shared_ptr<type::KThread>> queue; //!< A queue of threads which are running or to be run on this core

                explicit CoreContext(u8 id);
            };

            std::array<CoreContext, constant::CoreCount> cores{CoreContext(0), CoreContext(1), CoreContext(2), CoreContext(3)};

          public:
            Scheduler(const DeviceState &state);

            /**
             * @brief Checks all cores and migrates the calling thread to the core where the calling thread should be scheduled the earliest
             * @return A reference to the CoreContext of the core which the calling thread is running on after load balancing
             * @note This doesn't insert the thread into the migrated process's queue after load-balancing
             */
            CoreContext& LoadBalance();

            /**
             * @brief Inserts the calling thread into the scheduler queue at the appropriate location based on it's priority
             * @param loadBalance If to load balance or use the thread's current core (KThread::coreId)
             */
            void InsertThread(bool loadBalance = true);

            /**
             * @brief Wait for the current thread to be scheduled on it's resident core
             * @note There is an assumption of the thread being on it's resident core queue, if it's not this'll never return
             */
            void WaitSchedule();

            /**
             * @brief Rotates the calling thread's resident core queue, if it is at the front of it
             */
            void Rotate();

            /**
             * @brief Removes the calling thread from it's resident core queue
             */
            void RemoveThread();
        };
    }
}
