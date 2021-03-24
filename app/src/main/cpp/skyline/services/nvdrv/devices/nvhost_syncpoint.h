// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <soc/host1x.h>

namespace skyline::service::nvdrv {
    /**
     * @brief NvHostSyncpoint handles allocating and accessing host1x syncpoints, these are cached versions of the HW syncpoints which are intermittently synced
     * @note Refer to Chapter 14 of the Tegra X1 TRM for an exhaustive overview of them
     * @url https://http.download.nvidia.com/tegra-public-appnotes/host1x.html
     * @url https://github.com/Jetson-TX1-AndroidTV/android_kernel_jetson_tx1_hdmi_primary/blob/jetson-tx1/drivers/video/tegra/host/nvhost_syncpt.c
     */
    class NvHostSyncpoint {
      private:
        struct SyncpointInfo {
            std::atomic<u32> counterMin; //!< The least value the syncpoint can be (The value it was when it was last synchronized with host1x)
            std::atomic<u32> counterMax; //!< The maximum value the syncpoint can reach according to the current usage
            bool interfaceManaged;       //!< If the syncpoint is managed by a host1x client interface, a client interface is a HW block that can handle host1x transactions on behalf of a host1x client (Which would otherwise need to be manually synced using PIO which is synchronous and requires direct cooperation of the CPU)
            bool reserved;               //!< If the syncpoint is reserved or not, not to be confused with a reserved value
        };

        const DeviceState &state;
        std::array<SyncpointInfo, soc::host1x::SyncpointCount> syncpoints{};
        std::mutex reservationLock;

        /**
         * @note reservationLock should be locked when calling this
         */
        u32 ReserveSyncpoint(u32 id, bool clientManaged);

        /**
         * @return The ID of the first free syncpoint
         */
        u32 FindFreeSyncpoint();

      public:
        NvHostSyncpoint(const DeviceState &state);

        /**
         * @brief Finds a free syncpoint and reserves it
         * @return The ID of the reserved syncpoint
         */
        u32 AllocateSyncpoint(bool clientManaged);

        /**
         * @url https://github.com/Jetson-TX1-AndroidTV/android_kernel_jetson_tx1_hdmi_primary/blob/8f74a72394efb871cb3f886a3de2998cd7ff2990/drivers/gpu/host1x/syncpt.c#L259
         */
        bool HasSyncpointExpired(u32 id, u32 threshold);

        /**
         * @brief Atomically increments the maximum value of a syncpoint by the given amount
         * @return The new max value of the syncpoint
         */
        u32 IncrementSyncpointMaxExt(u32 id, u32 amount);

        /**
         * @return The minimum value of the syncpoint
         */
        u32 ReadSyncpointMinValue(u32 id);

        /**
         * @brief Synchronises the minimum value of the syncpoint to with the GPU
         * @return The new minimum value of the syncpoint
         */
        u32 UpdateMin(u32 id);
    };
}
