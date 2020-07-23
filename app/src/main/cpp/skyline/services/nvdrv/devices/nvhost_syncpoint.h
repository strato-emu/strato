// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <array>

namespace skyline {
    namespace constant {
        constexpr size_t MaxHwSyncpointCount = 192;
    }

    namespace service::nvdrv {
        /**
         * @todo Implement the GPU side of this
         * @brief NvHostSyncpoint handles allocating and accessing host1x syncpoints
         * @url https://http.download.nvidia.com/tegra-public-appnotes/host1x.html
         * @url https://github.com/Jetson-TX1-AndroidTV/android_kernel_jetson_tx1_hdmi_primary/blob/jetson-tx1/drivers/video/tegra/host/nvhost_syncpt.c
         */
        class NvHostSyncpoint {
          private:
            /**
            * @brief This holds information about a single syncpoint
            */
            struct SyncpointInfo {
                std::atomic<u32> counterMin;
                std::atomic<u32> counterMax;
                bool clientManaged;
                bool assigned;
            };

            const DeviceState &state;
            std::array<SyncpointInfo, skyline::constant::MaxHwSyncpointCount> syncpoints{};
            Mutex reservationLock;

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
             * @return The new value of the syncpoint
             */
            u32 IncrementSyncpointMaxExt(u32 id, u32 amount);
        };
    }
}
