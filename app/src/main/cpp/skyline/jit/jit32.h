// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#pragma once

#include <array>
#include <dynarmic/interface/exclusive_monitor.h>
#include "common.h"
#include "jit_core_32.h"

namespace skyline::jit {
    constexpr auto CoreCount{4};

    /**
     * @brief The JIT for the 32-bit ARM CPU
     */
    class Jit32 {
      public:
        explicit Jit32(DeviceState &state);

        /**
         * @brief Gets the JIT core for the specified core ID
         */
        JitCore32 &GetCore(u32 coreId);

        /**
         * @brief Handles any signals in the JIT threads
         */
        static void SignalHandler(int signal, siginfo *info, ucontext *ctx);

      private:
        DeviceState &state;

        Dynarmic::ExclusiveMonitor monitor;
        std::array<jit::JitCore32, CoreCount> cores;
    };
}
