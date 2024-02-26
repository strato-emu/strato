// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#include "jit32.h"

namespace skyline::jit {
    Jit32::Jit32(DeviceState &state)
        : state{state},
          cores{jit::JitCore32(state, 0),
                jit::JitCore32(state, 1),
                jit::JitCore32(state, 2),
                jit::JitCore32(state, 3)} {}

    JitCore32 &Jit32::GetCore(u32 coreId) {
        return cores[coreId];
    }
}
