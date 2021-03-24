// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "gm20b/engines/maxwell_3d.h"
#include "gm20b/gpfifo.h"

namespace skyline::soc::gm20b {
    /**
     * @brief The GPU block in the X1, it contains all GPU engines required for accelerating graphics operations
     * @note We omit parts of components related to external access such as the GM20B Host, all accesses to the external components are done directly
     */
    class GM20B {
      public:
        engine::Engine fermi2D;
        engine::maxwell3d::Maxwell3D maxwell3D;
        engine::Engine maxwellCompute;
        engine::Engine maxwellDma;
        engine::Engine keplerMemory;
        GPFIFO gpfifo;

        GM20B(const DeviceState &state) : fermi2D(state), keplerMemory(state), maxwell3D(state), maxwellCompute(state), maxwellDma(state), gpfifo(state) {}
    };
}
