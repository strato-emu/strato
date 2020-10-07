// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <cstdint>

#define FORCE_INLINE __attribute__((always_inline)) inline // NOLINT(cppcoreguidelines-macro-usage)

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

    /**
     * @brief The state of all the general purpose registers in the guest
     * @note Read about ARMv8 registers here: https://developer.arm.com/docs/100878/latest/registers
     * @note X30 or LR is not provided as it is reserved for other uses
     */
    union Registers {
        u64 regs[30];
        struct {
            u64 x0;
            u64 x1;
            u64 x2;
            u64 x3;
            u64 x4;
            u64 x5;
            u64 x6;
            u64 x7;
            u64 x8;
            u64 x9;
            u64 x10;
            u64 x11;
            u64 x12;
            u64 x13;
            u64 x14;
            u64 x15;
            u64 x16;
            u64 x17;
            u64 x18;
            u64 x19;
            u64 x20;
            u64 x21;
            u64 x22;
            u64 x23;
            u64 x24;
            u64 x25;
            u64 x26;
            u64 x27;
            u64 x28;
            u64 x29;
        };
        struct {
            u32 w0;
            u32 __w0__;
            u32 w1;
            u32 __w1__;
            u32 w2;
            u32 __w2__;
            u32 w3;
            u32 __w3__;
            u32 w4;
            u32 __w4__;
            u32 w5;
            u32 __w5__;
            u32 w6;
            u32 __w6__;
            u32 w7;
            u32 __w7__;
            u32 w8;
            u32 __w8__;
            u32 w9;
            u32 __w9__;
            u32 w10;
            u32 __w10__;
            u32 w11;
            u32 __w11__;
            u32 w12;
            u32 __w12__;
            u32 w13;
            u32 __w13__;
            u32 w14;
            u32 __w14__;
            u32 w15;
            u32 __w15__;
            u32 w16;
            u32 __w16__;
            u32 w17;
            u32 __w17__;
            u32 w18;
            u32 __w18__;
            u32 w19;
            u32 __w19__;
            u32 w20;
            u32 __w20__;
            u32 w21;
            u32 __w21__;
            u32 w22;
            u32 __w22__;
            u32 w23;
            u32 __w23__;
            u32 w24;
            u32 __w24__;
            u32 w25;
            u32 __w25__;
            u32 w26;
            u32 __w26__;
            u32 w27;
            u32 __w27__;
            u32 w28;
            u32 __w28__;
            u32 w29;
            u32 __w29__;
        };
    };

    class NCE;

    /**
     * @brief The context of a thread during kernel calls, it is stored for each thread
     */
    struct ThreadContext {
        u8* pc; //!< The program counter on the guest
        u8* sp; //!< The stack pointer on the guest
        Registers registers; //!< The general purpose registers on the guest
        u8* tpidrroEl0; //!< The value for TPIDRRO_EL0 for the current thread
        u8* tpidrEl0; //!< The value for TPIDR_EL0 for the current thread
        NCE* nce; //!< An instance of the NCE class, used by trampoline functions to call class methods
    };
}
