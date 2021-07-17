// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "nvmap.h"
#include "syncpoint_manager.h"

namespace skyline::service::nvdrv {
    /**
     * @brief Holds the global state of nvdrv
     */
    struct Core {
        core::NvMap nvMap;
        core::SyncpointManager syncpointManager;

        Core(const DeviceState &state) : nvMap(state), syncpointManager(state) {}
    };
}