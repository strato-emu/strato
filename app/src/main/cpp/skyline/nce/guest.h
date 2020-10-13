// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline {
    struct DeviceState;
    namespace nce {
        /**
         * @brief The state of callee-saved general purpose registers in the guest
         * @note Read about ARMv8 registers here: https://developer.arm.com/architectures/learn-the-architecture/armv8-a-instruction-set-architecture/registers-in-aarch64-general-purpose-registers
         * @note Read about ARMv8 ABI here: https://github.com/ARM-software/abi-aa/blob/2f1ac56a7d79f3e753e6ca88d4d3e083c31d6f64/aapcs64/aapcs64.rst#machine-registers
         */
        union GpRegisters {
            std::array<u64, 19> regs;
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
            };
        };

        /**
         * @brief The state of all floating point (and SIMD) registers in the guest
         * @note FPSR/FPCR are 64-bit system registers but only the lower 32-bits are used
         * @note Read about ARMv8 ABI here: https://github.com/ARM-software/abi-aa/blob/2f1ac56a7d79f3e753e6ca88d4d3e083c31d6f64/aapcs64/aapcs64.rst#612simd-and-floating-point-registers
         */
        union alignas(16) FpRegisters {
            std::array<u128, 32> regs;
            u32 fpsr;
            u32 fpcr;
        };

        /**
         * @brief A per-thread context for guest threads
         * @note It's stored in TPIDR_EL0 while running the guest
         */
        struct ThreadContext {
            GpRegisters gpr;
            FpRegisters fpr;
            u8 *hostTpidrEl0; //!< Host TLS TPIDR_EL0, this must be swapped to prior to calling any CXX functions
            u8 *hostSp; //!< Host Stack Pointer, same as above
            u8 *tpidrroEl0; //!< Emulated HOS TPIDRRO_EL0
            u8 *tpidrEl0; //!< Emulated HOS TPIDR_EL0
            const DeviceState *state;
        };

        namespace guest {
            constexpr size_t SaveCtxSize{39}; //!< The size of the SaveCtx function in 32-bit ARMv8 instructions
            constexpr size_t LoadCtxSize{39}; //!< The size of the LoadCtx function in 32-bit ARMv8 instructions
            constexpr size_t RescaleClockSize{16}; //!< The size of the RescaleClock function in 32-bit ARMv8 instructions

            /**
             * @brief Saves the context from CPU registers into TLS
             * @note Assumes that 8B is reserved at an offset of 8B from SP
             */
            extern "C" void SaveCtx(void);

            /**
             * @brief Loads the context from TLS into CPU registers
             * @note Assumes that 8B is reserved at an offset of 8B from SP
             */
            extern "C" void LoadCtx(void);

            /**
             * @brief Rescales the host clock to Tegra X1 levels
             * @note Output is on stack with the stack pointer offset 32B from the initial point
             */
            extern "C" __noreturn void RescaleClock(void);
        }
    }
}
