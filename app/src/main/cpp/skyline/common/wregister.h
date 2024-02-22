// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#pragma once

#include <common/base.h>

namespace skyline {
    struct WRegister {
        u32 lower;
        u32 upper;

        constexpr operator u32() const {
            return lower;
        }

        void operator=(u32 value) {
            lower = value;
            upper = 0;
        }
    };
}
