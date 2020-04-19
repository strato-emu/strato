// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "memory_pool.h"

namespace skyline::service::audio::IAudioRenderer {
    void MemoryPool::ProcessInput(const MemoryPoolIn &input) {
        if (input.state == MemoryPoolState::RequestAttach)
            output.state = MemoryPoolState::Attached;
        else if (input.state == MemoryPoolState::RequestDetach)
            output.state = MemoryPoolState::Detached;
    }
}
