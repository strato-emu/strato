// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/address_space.h>

namespace skyline::soc {
    static constexpr u8 SmmuAddressSpaceBits{32}; //!< The size of the SMMU AS in bits

    /**
     * @brief The SMMU (System Memory Management Unit) class handles mapping between the host1x peripheral virtual address space and an application's address space
     * @note The SMMU is implemented entirely as a template specialization over FlatMemoryManager
     */
    using SMMU = FlatMemoryManager<u32, 0, SmmuAddressSpaceBits>;
}
