// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/address_space.h>
#include "gm20b/engines/maxwell_3d.h"
#include "gm20b/gpfifo.h"

namespace skyline::soc::gm20b {
    /**
     * @brief The GPU block in the X1, it contains all GPU engines required for accelerating graphics operations
     * @note We omit parts of components related to external access such as the grhost, all accesses to the external components are done directly
     */
    class GM20B {
      public:
        static constexpr u8 AddressSpaceBits{40}; //!< The width of the GMMU AS
        using GMMU = FlatMemoryManager<u64, 0, AddressSpaceBits>;

        engine::Engine fermi2D;
        engine::maxwell3d::Maxwell3D maxwell3D;
        engine::Engine maxwellCompute;
        engine::Engine maxwellDma;
        engine::Engine keplerMemory;
        GPFIFO gpfifo;
        GMMU gmmu;

        GM20B(const DeviceState &state);
    };
}
