// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2024 Strato Team and Contributors (https://github.com/strato-emu/)

#pragma once

#include <functional>
#include <mutex>
#include "interval_map.h"

namespace skyline {
    /**
     * @brief The level of protection that is required for a callback entry
     */
    enum class TrapProtection {
        None = 0, //!< No protection is required
        WriteOnly = 1, //!< Only write protection is required
        ReadWrite = 2, //!< Both read and write protection are required
    };

    using TrapCallback = std::function<bool()>;
    using LockCallback = std::function<void()>;

    struct CallbackEntry {
        TrapProtection protection; //!< The least restrictive protection that this callback needs to have
        LockCallback lockCallback;
        TrapCallback readCallback, writeCallback;

        CallbackEntry(TrapProtection protection, LockCallback lockCallback, TrapCallback readCallback, TrapCallback writeCallback);
    };

    using TrapMap = IntervalMap<u8 *, CallbackEntry>;

    /**
     * @brief An opaque handle to a group of trapped region
     */
    class TrapHandle : private TrapMap::GroupHandle {
        constexpr TrapHandle(const TrapMap::GroupHandle &handle);

        friend class TrapManager;
    };

    class TrapManager {
      public:
        /**
         * @brief Creates a region of guest memory that can be trapped with a callback for when an access to it has been made
         * @param lockCallback A callback to lock the resource that is being trapped, it must block until the resource is locked but unlock it prior to returning
         * @param readCallback A callback for read accesses to the trapped region, it must not block and return a boolean if it would block
         * @param writeCallback A callback for write accesses to the trapped region, it must not block and return a boolean if it would block
         * @note The handle **must** be deleted using DeleteTrap before the NCE instance is destroyed
         * @note It is UB to supply a region of host memory rather than guest memory
         * @note This doesn't trap the region in itself, any trapping must be done via TrapRegions(...)
         */
        TrapHandle CreateTrap(span<span<u8>> regions, const LockCallback &lockCallback, const TrapCallback &readCallback, const TrapCallback &writeCallback);

        /**
         * @brief Re-traps a region of memory after protections were removed
         * @param writeOnly If the trap is optimally for write-only accesses, this is not guarenteed
         */
        void TrapRegions(TrapHandle handle, bool writeOnly);

        /**
         * @brief Removes protections from a region of memory
         */
        void RemoveTrap(TrapHandle handle);

        /**
         * @brief Deletes a trap handle and removes the protection from the region
         */
        void DeleteTrap(TrapHandle handle);

        /**
         * @brief Handles a trap
         * @param address The address that was trapped
         * @param write If the access was a write
         * @return If the access should be allowed
         */
        bool HandleTrap(u8 *address, bool write);

        /**
         * @brief Installs this instance as the static instance used by the trap handler
         */
        void InstallStaticInstance();

        /**
         * @brief The trap manager handler function
         */
        static bool TrapHandler(u8 *address, bool write);

      private:
        /**
         * @brief Reprotects the intervals to the least restrictive protection given the supplied protection
         */
        void ReprotectIntervals(const std::vector<TrapMap::Interval> &intervals, TrapProtection protection);

      private:
        std::mutex trapMutex; //!< Synchronizes the accesses to the trap map
        TrapMap trapMap; //!< A map of all intervals and corresponding callbacks that have been registered
    };
}
