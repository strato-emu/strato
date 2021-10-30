// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::soc::host1x {
    /**
     * @note See '14.4.10 Class IDs' in the TRM
     */
    enum class ClassId : u16 {
        Host1x = 0x1,
        VIC = 0x5D,
        NvJpg = 0xC0,
        NvDec = 0xF0
    };

    constexpr static u32 IncrementSyncpointMethodId{0}; //!< See below

    /**
     * @note This method is common between all classes
     * @note This is derived from '14.10.1 NV_CLASS_HOST_INCR_SYNCPT_0' in the TRM
     */
    union IncrementSyncpointMethod {
        enum class Condition : u8 {
            Immediate = 0,
            OpDone = 1,
            RdDone = 2,
            RegWRSafe = 3
        };

        u32 raw;

        struct {
            u8 index;
            Condition condition;
            u16 _pad_;
        };
    };
    static_assert(sizeof(IncrementSyncpointMethod) == sizeof(u32));
}
