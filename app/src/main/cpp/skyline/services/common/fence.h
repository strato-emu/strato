// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <services/nvdrv/devices/nvhost_syncpoint.h>

namespace skyline::service::nvdrv {
    /**
     * @brief This holds information about a fence
     */
    struct Fence {
        u32 id{};
        u32 value{};

        /**
         * @brief Synchronizes the fence's value with its underlying syncpoint
         */
        inline void UpdateValue(NvHostSyncpoint &hostSyncpoint) {
            value = hostSyncpoint.UpdateMin(id);
        }
    };
    static_assert(sizeof(Fence) == 0x8);
}
