// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <bit>
#include <common/address_space.h>

namespace skyline::soc::gm20b {
    static constexpr u8 GmmuAddressSpaceBits{40}; //!< The size of the GMMU AS in bits
    static constexpr size_t GmmuSmallPageSize{0x1000}; // 4KiB
    static constexpr size_t GmmuSmallPageSizeBits{std::countr_zero(GmmuSmallPageSize)};
    static constexpr size_t GmmuMinBigPageSize{0x20000}; // 128KiB
    static constexpr size_t GmmuMinBigPageSizeBits{std::countr_zero(GmmuMinBigPageSize)};


    /**
     * @brief The GMMU (Graphics Memory Management Unit) class handles mapping between a Maxwell GPU virtual address space and an application's address space and is meant to roughly emulate the GMMU on the X1
     * @note This is not accurate to the X1 as it would have an SMMU between the GMMU and physical memory but we don't need to emulate this abstraction
     * @note The GMMU is implemented entirely as a template specialization over FlatMemoryManager
     */
    using GMMU = FlatMemoryManager<u64, 0, GmmuAddressSpaceBits, GmmuSmallPageSizeBits, GmmuMinBigPageSizeBits>;

    struct AddressSpaceContext {
        GMMU gmmu;
    };

    /**
     * @brief A host IOVA address composed of 32-bit low/high register values
     * @note This differs from engine::Address in that it is little-endian rather than big-endian ordered for the register values
     */
    union IOVA {
        u64 iova;
        struct {
            u32 low;
            u32 high;
        };

        IOVA(u64 iova) : iova{iova} {}

        operator u64 &() {
            return iova;
        }
    };
    static_assert(sizeof(IOVA) == sizeof(u64));
}
