// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/address_space.h>

namespace skyline::soc {
    static constexpr u8 SmmuAddressSpaceBits{32}; //!< The size of the SMMU AS in bits
    constexpr size_t SmmuPageSize{0x1000}; // 4KiB
    constexpr size_t SmmuPageSizeBits{std::countr_zero(SmmuPageSize)};
    constexpr size_t SmmuL2PageSize{0x20000}; // 128KiB - not actually a thing in HW but needed for segment table
    constexpr size_t SmmuL2PageSizeBits{std::countr_zero(SmmuL2PageSize)};



    /**
     * @brief The SMMU (System Memory Management Unit) class handles mapping between the host1x peripheral virtual address space and an application's address space
     * @note The SMMU is implemented entirely as a template specialization over FlatMemoryManager
     */
    using SMMU = FlatMemoryManager<u32, 0, SmmuAddressSpaceBits, SmmuPageSizeBits, SmmuL2PageSizeBits>;
}
