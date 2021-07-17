// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

namespace skyline::service::nvdrv {
    /**
     * @brief A Fence is a synchronization primitive that describes a point in a Syncpoint to synchronize at
     */
    struct Fence {
        u32 id{}; //!< The ID of the underlying syncpoint
        u32 threshold{}; //!< The value of the syncpoint at which the fence is signalled
    };
    static_assert(sizeof(Fence) == 0x8);
}
