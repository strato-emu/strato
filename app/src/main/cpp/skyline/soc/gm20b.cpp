// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "gm20b.h"

namespace skyline::soc::gm20b {
    GM20B::GM20B(const DeviceState &state) :
        fermi2D(state),
        keplerMemory(state),
        maxwell3D(state, gmmu),
        maxwellCompute(state),
        maxwellDma(state),
        gpfifo(state) {}
}
