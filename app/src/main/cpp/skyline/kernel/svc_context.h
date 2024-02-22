// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#pragma once

#include <common/base.h>
#include <common/wregister.h>

namespace skyline::kernel::svc {
    /**
     * @brief Register context for SVCs
     * @note This is used to abstract register access for SVCs, allowing for them to be called seamlessly from NCE or JIT
     * @details Binary layout of this struct must be kept equal to NCE's ThreadContext
     */
    struct SvcContext {
        union {
            std::array<u64, 6> regs;
            struct {
                u64 x0, x1, x2, x3, x4, x5;
            };
            struct {
                WRegister w0, w1, w2, w3, w4, w5;
            };
        };
    };
}
