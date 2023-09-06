// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <cstdint>
#include <variant>

namespace skyline {
    using u128 = __uint128_t; //!< Unsigned 128-bit integer
    using u64 = __uint64_t; //!< Unsigned 64-bit integer
    using u32 = __uint32_t; //!< Unsigned 32-bit integer
    using u16 = __uint16_t; //!< Unsigned 16-bit integer
    using u8 = __uint8_t; //!< Unsigned 8-bit integer
    using i128 = __int128_t; //!< Signed 128-bit integer
    using i64 = __int64_t; //!< Signed 64-bit integer
    using i32 = __int32_t; //!< Signed 32-bit integer
    using i16 = __int16_t; //!< Signed 16-bit integer
    using i8 = __int8_t; //!< Signed 8-bit integer

    using KHandle = u32; //!< The type of a kernel handle

    namespace constant {
        // Time
        constexpr i64 NsInMicrosecond{1000}; //!< The amount of nanoseconds in a microsecond
        constexpr i64 NsInSecond{1000000000}; //!< The amount of nanoseconds in a second
        constexpr i64 NsInMillisecond{1000000}; //!< The amount of nanoseconds in a millisecond
        constexpr i64 NsInDay{86400000000000UL}; //!< The amount of nanoseconds in a day

        constexpr size_t AddressSpaceSize{1ULL << 39}; //!< The size of the host CPU AS in bytes
        constexpr size_t PageSize{0x1000}; //!< The size of a host page
        constexpr size_t PageSizeBits{12}; //!< log2(PageSize)

        static_assert(PageSize == PAGE_SIZE);
    }

    /**
     * @brief A deduction guide for overloads required for std::visit with std::variant
     */
    template<class... Ts>
    struct VariantVisitor : Ts ... { using Ts::operator()...; };
    template<class... Ts> VariantVisitor(Ts...) -> VariantVisitor<Ts...>;
}
