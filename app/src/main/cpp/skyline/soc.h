// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "soc/gmmu.h"
#include "soc/host1x.h"
#include "soc/gm20b.h"

namespace skyline::soc {
    /**
     * @brief An interface into all emulated components of the Tegra X1 SoC
     * @note Refer to the Tegra X1 Processor Block Diagram (1.2) for more information
     */
    class SOC {
      public:
        gmmu::GraphicsMemoryManager gmmu;
        host1x::Host1X host1x;
        gm20b::GM20B gm20b;

        SOC(const DeviceState &state) : gmmu(state), gm20b(state) {}
    };
}
