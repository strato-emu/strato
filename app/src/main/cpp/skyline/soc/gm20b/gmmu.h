// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/address_space.h>

namespace skyline::soc::gm20b {
    static constexpr u8 GmmuAddressSpaceBits{40}; //!< The size of the GMMU AS in bits

    /**
     * @brief The GMMU (Graphics Memory Management Unit) class handles mapping between a Maxwell GPU virtual address space and an application's address space and is meant to roughly emulate the GMMU on the X1
     * @note This is not accurate to the X1 as it would have an SMMU between the GMMU and physical memory but we don't emulate this abstraction at the moment
     * @note The GMMU is implemented entirely as a template specialization over FlatMemoryManager
     */
    using GMMU = FlatMemoryManager<u64, 0, GmmuAddressSpaceBits>;
}
