// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/address_space.inc>
#include "gmmu.h"

namespace skyline {
    template class FlatAddressSpaceMap<u64, 0, u8 *, nullptr, true, soc::gm20b::GmmuAddressSpaceBits>;
    template class FlatMemoryManager<u64, 0, soc::gm20b::GmmuAddressSpaceBits, soc::gm20b::GmmuSmallPageSizeBits, soc::gm20b::GmmuMinBigPageSizeBits>;
}
