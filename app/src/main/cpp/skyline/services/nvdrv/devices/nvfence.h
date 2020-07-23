// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include "nvhost_syncpoint.h"

namespace skyline::service::nvdrv::device {
    /**
     * @brief This holds information about a fence
     */
    struct NvFence {
        u32 id{};
        u32 value{};

        /**
         * @brief Synchronizes the fence's value with its underlying syncpoint
         */
        static inline void UpdateValue(const NvHostSyncpoint &hostSyncpoint) {
            //TODO: Implement this
        }
    };
    static_assert(sizeof(NvFence) == 0x8);
}
