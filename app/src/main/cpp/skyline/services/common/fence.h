// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/nvdrv/devices/nvhost_syncpoint.h>

namespace skyline::service::nvdrv {
    /**
     * @brief A Fence is a synchronization primitive that describes a point in a Syncpoint to synchronize at
     */
    struct Fence {
        u32 id{}; //!< The ID of the underlying syncpoint
        u32 value{}; //!< The value of the syncpoint at which the fence is passed

        /**
         * @brief Synchronizes the fence's value with its underlying syncpoint
         */
        void UpdateValue(NvHostSyncpoint &hostSyncpoint) {
            value = hostSyncpoint.UpdateMin(id);
        }
    };
    static_assert(sizeof(Fence) == 0x8);
}
