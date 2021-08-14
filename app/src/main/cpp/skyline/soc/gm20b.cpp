// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/address_space.inc>
#include "gm20b.h"

namespace skyline {
    template class FlatAddressSpaceMap<u64, 0, u8 *, nullptr, true, soc::gm20b::GM20B::AddressSpaceBits>;
    template class FlatMemoryManager<u64, 0, soc::gm20b::GM20B::AddressSpaceBits>;
}

namespace skyline::soc::gm20b {
    GM20B::GM20B(const DeviceState &state) :
        fermi2D(state),
        keplerMemory(state),
        maxwell3D(state),
        maxwellCompute(state),
        maxwellDma(state),
        gpfifo(state) {}
}
