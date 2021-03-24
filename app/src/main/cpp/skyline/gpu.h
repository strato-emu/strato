// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "gpu/presentation_engine.h"

namespace skyline::gpu {
    /**
     * @brief An interface to host GPU structures, anything concerning host GPU/Presentation APIs is encapsulated by this
     */
    class GPU {
      public:
        PresentationEngine presentation;

        GPU(const DeviceState &state) : presentation(state) {}
    };
}
