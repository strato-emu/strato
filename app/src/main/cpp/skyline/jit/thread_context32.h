// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#pragma once

#include <common/base.h>
#include <kernel/svc_context.h>

namespace skyline::jit {
    class JitCore32; // Forward declaration

    /**
     * @brief The register context of a thread running in 32-bit mode
     */
    struct ThreadContext32 {
        union {
            std::array<u32, 16> gpr{}; //!< General purpose registers
            struct {
                std::array<u32, 13> _pad_;
                u32 sp;
                u32 lr;
                u32 pc;
            };
        };
        u32 cpsr{0}; //!< Current program status register
        u32 pad{0};
        union {
            std::array<u32, 64> fpr{}; //!< Floating point and vector registers
            std::array<u64, 32> fpr_d; //!< Floating point and vector registers as double words
        };
        u32 fpscr{0}; //!< Floating point status and control register
        u32 tpidr{0}; //!< Thread ID register
    };
    static_assert(sizeof(ThreadContext32) == 0x150, "ThreadContext32 should be 336 bytes in size to match HOS");

    /**
     * @brief Creates a new SvcContext from the given JIT32 thread context
     */
    inline kernel::svc::SvcContext MakeSvcContext(const ThreadContext32 &threadctx) {
        kernel::svc::SvcContext ctx;
        for (size_t i = 0; i < ctx.regs.size(); i++)
            ctx.regs[i] = static_cast<u64>(threadctx.gpr[i]);
        return ctx;
    }

    /**
     * @brief Applies changes from the given SvcContext to the JIT32 thread context
     */
    inline void ApplySvcContext(const kernel::svc::SvcContext &svcCtx, ThreadContext32 &threadctx) {
        for (size_t i = 0; i < svcCtx.regs.size(); i++)
            threadctx.gpr[i] = static_cast<u32>(svcCtx.regs[i]);
    }
}
