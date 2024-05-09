// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#include "jit32.h"
#include <common/trap_manager.h>
#include <common/signal.h>
#include <kernel/types/KThread.h>
#include <kernel/types/KProcess.h>
#include <loader/loader.h>

namespace skyline::jit {
    static std::array<JitCore32, CoreCount> MakeJitCores(const DeviceState &state, Dynarmic::ExclusiveMonitor &monitor) {
        // Set the signal handler before creating the JIT cores to ensure proper chaining with the Dynarmic handler which is set during construction
        signal::SetHostSignalHandler({SIGINT, SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV}, Jit32::SignalHandler);

        return {JitCore32(state, monitor, 0),
                JitCore32(state, monitor, 1),
                JitCore32(state, monitor, 2),
                JitCore32(state, monitor, 3)};
    }

    Jit32::Jit32(DeviceState &state)
        : state{state},
          monitor{CoreCount},
          cores{MakeJitCores(state, monitor)} {}

    JitCore32 &Jit32::GetCore(u32 coreId) {
        return cores[coreId];
    }

    void Jit32::SignalHandler(int signal, siginfo *info, ucontext *ctx) {
        if (signal == SIGSEGV)
            // Handle any accesses that may be from a trapped region
            if (TrapManager::TrapHandler(reinterpret_cast<u8 *>(info->si_addr), true))
                return;

        auto &mctx{ctx->uc_mcontext};
        auto thread{kernel::this_thread};
        bool isGuest{thread->jit != nullptr}; // Whether the signal happened while running guest code

        if (isGuest) {
            if (signal != SIGINT) {
                signal::StackFrame topFrame{.lr = reinterpret_cast<void *>(ctx->uc_mcontext.pc), .next = reinterpret_cast<signal::StackFrame *>(ctx->uc_mcontext.regs[29])};
                // TODO: this might give garbage stack frames and/or crash
                std::string trace{thread->process.state.loader->GetStackTrace(&topFrame)};

                std::string cpuContext;
                if (mctx.fault_address)
                    cpuContext += fmt::format("\n  Fault Address: 0x{:X}", mctx.fault_address);
                if (mctx.sp)
                    cpuContext += fmt::format("\n  Stack Pointer: 0x{:X}", mctx.sp);
                for (size_t index{}; index < (sizeof(mcontext_t::regs) / sizeof(u64)); index += 2)
                    cpuContext += fmt::format("\n  X{:<2}: 0x{:<16X} X{:<2}: 0x{:X}", index, mctx.regs[index], index + 1, mctx.regs[index + 1]);

                LOGE("Thread #{} has crashed due to signal: {}\nStack Trace:{} \nCPU Context:{}", thread->id, strsignal(signal), trace, cpuContext);

                if (thread->id) {
                    signal::BlockSignal({SIGINT});
                    thread->process.Kill(false);
                }
            }

            mctx.pc = reinterpret_cast<u64>(&std::longjmp);
            mctx.regs[0] = reinterpret_cast<u64>(thread->originalCtx);
            mctx.regs[1] = true;
        } else {
            signal::ExceptionalSignalHandler(signal, info, ctx); // Delegate throwing a host exception to the exceptional signal handler
        }
    }
}
