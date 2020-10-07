// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "guest_common.h"

namespace skyline {
    namespace guest {
        constexpr size_t SaveCtxSize{20 * sizeof(u32)}; //!< The size of the SaveCtx function in 32-bit ARMv8 instructions
        constexpr size_t LoadCtxSize{20 * sizeof(u32)}; //!< The size of the LoadCtx function in 32-bit ARMv8 instructions
        constexpr size_t RescaleClockSize{16 * sizeof(u32)}; //!< The size of the RescaleClock function in 32-bit ARMv8 instructions
        #ifdef NDEBUG
        constexpr size_t SvcHandlerSize{225 * sizeof(u32)}; //!< The size of the SvcHandler (Release) function in 32-bit ARMv8 instructions
        #else
        constexpr size_t SvcHandlerSize{400 * sizeof(u32)}; //!< The size of the SvcHandler (Debug) function in 32-bit ARMv8 instructions
        #endif

        /**
         * @brief Saves the context from CPU registers into TLS
         */
        extern "C" void SaveCtx(void);

        /**
         * @brief Loads the context from TLS into CPU registers
         */
        extern "C" void LoadCtx(void);

        /**
         * @brief Rescales the clock to Tegra X1 levels and puts the output on stack
         */
        extern "C" __noreturn void RescaleClock(void);

        /**
         * @brief Handles all SVC calls
         * @param pc The address of PC when the call was being done
         * @param svc The SVC ID of the SVC being called
         */
        void SvcHandler(u64 pc, u16 svc);
    }
}
