#include <asm/siginfo.h>
#include <csignal>
#include <cstdlib>
#include <initializer_list> // This is used implicitly
#include "guest_common.h"

#define FORCE_INLINE __attribute__((always_inline)) inline // NOLINT(cppcoreguidelines-macro-usage)

namespace skyline::guest {
    FORCE_INLINE void saveCtxStack() {
        asm("SUB SP, SP, #240\n\t"
            "STP X0, X1, [SP, #0]\n\t"
            "STP X2, X3, [SP, #16]\n\t"
            "STP X4, X5, [SP, #32]\n\t"
            "STP X6, X7, [SP, #48]\n\t"
            "STP X8, X9, [SP, #64]\n\t"
            "STP X10, X11, [SP, #80]\n\t"
            "STP X12, X13, [SP, #96]\n\t"
            "STP X14, X15, [SP, #112]\n\t"
            "STP X16, X17, [SP, #128]\n\t"
            "STP X18, X19, [SP, #144]\n\t"
            "STP X20, X21, [SP, #160]\n\t"
            "STP X22, X23, [SP, #176]\n\t"
            "STP X24, X25, [SP, #192]\n\t"
            "STP X26, X27, [SP, #208]\n\t"
            "STP X28, X29, [SP, #224]"
        );
    }

    FORCE_INLINE void loadCtxStack() {
        asm("LDP X0, X1, [SP, #0]\n\t"
            "LDP X2, X3, [SP, #16]\n\t"
            "LDP X4, X5, [SP, #32]\n\t"
            "LDP X6, X7, [SP, #48]\n\t"
            "LDP X8, X9, [SP, #64]\n\t"
            "LDP X10, X11, [SP, #80]\n\t"
            "LDP X12, X13, [SP, #96]\n\t"
            "LDP X14, X15, [SP, #112]\n\t"
            "LDP X16, X17, [SP, #128]\n\t"
            "LDP X18, X19, [SP, #144]\n\t"
            "LDP X20, X21, [SP, #160]\n\t"
            "LDP X22, X23, [SP, #176]\n\t"
            "LDP X24, X25, [SP, #192]\n\t"
            "LDP X26, X27, [SP, #208]\n\t"
            "LDP X28, X29, [SP, #224]\n\t"
            "ADD SP, SP, #240"
        );
    }

    FORCE_INLINE void saveCtxTls() {
        asm("STR LR, [SP, #-16]!\n\t"
            "MRS LR, TPIDR_EL0\n\t"
            "STP X0, X1, [LR, #16]\n\t"
            "STP X2, X3, [LR, #32]\n\t"
            "STP X4, X5, [LR, #48]\n\t"
            "STP X6, X7, [LR, #64]\n\t"
            "STP X8, X9, [LR, #80]\n\t"
            "STP X10, X11, [LR, #96]\n\t"
            "STP X12, X13, [LR, #112]\n\t"
            "STP X14, X15, [LR, #128]\n\t"
            "STP X16, X17, [LR, #144]\n\t"
            "STP X18, X19, [LR, #160]\n\t"
            "STP X20, X21, [LR, #176]\n\t"
            "STP X22, X23, [LR, #192]\n\t"
            "STP X24, X25, [LR, #208]\n\t"
            "STP X26, X27, [LR, #224]\n\t"
            "STP X28, X29, [LR, #240]\n\t"
            "LDR LR, [SP], #16"
        );
    }

    FORCE_INLINE void loadCtxTls() {
        asm("STR LR, [SP, #-16]!\n\t"
            "MRS LR, TPIDR_EL0\n\t"
            "LDP X0, X1, [LR, #16]\n\t"
            "LDP X2, X3, [LR, #32]\n\t"
            "LDP X4, X5, [LR, #48]\n\t"
            "LDP X6, X7, [LR, #64]\n\t"
            "LDP X8, X9, [LR, #80]\n\t"
            "LDP X10, X11, [LR, #96]\n\t"
            "LDP X12, X13, [LR, #112]\n\t"
            "LDP X14, X15, [LR, #128]\n\t"
            "LDP X16, X17, [LR, #144]\n\t"
            "LDP X18, X19, [LR, #160]\n\t"
            "LDP X20, X21, [LR, #176]\n\t"
            "LDP X22, X23, [LR, #192]\n\t"
            "LDP X24, X25, [LR, #208]\n\t"
            "LDP X26, X27, [LR, #224]\n\t"
            "LDP X28, X29, [LR, #240]\n\t"
            "LDR LR, [SP], #16"
        );
    }

    void svcHandler(u64 pc, u32 svc) {
        volatile ThreadContext *ctx;
        asm("MRS %0, TPIDR_EL0":"=r"(ctx));
        ctx->pc = pc;
        ctx->commandId = svc;
        if (svc == 0xB) { // svcSleepThread
            switch (ctx->registers.x0) {
                case 0:
                case 1:
                case 2: {
                    asm("MOV X0, XZR\n\t"
                        "MOV X1, XZR\n\t"
                        "MOV X2, XZR\n\t"
                        "MOV X3, XZR\n\t"
                        "MOV X4, XZR\n\t"
                        "MOV X5, XZR\n\t"
                        "MOV X8, #124\n\t" // __NR_sched_yield
                        "STR LR, [SP, #-16]!\n\t"
                        "MOV LR, SP\n\t"
                        "SVC #0\n\t"
                        "MOV SP, LR\n\t"
                        "LDR LR, [SP], #16" ::: "x0", "x1", "x2", "x3", "x4", "x5", "x8");
                    break;
                }
                default: {
                    struct timespec spec = {
                        .tv_sec = static_cast<time_t>(ctx->registers.x0 / 1000000000),
                        .tv_nsec = static_cast<long>(ctx->registers.x0 % 1000000000)
                    };
                    asm("MOV X0, %0\n\t"
                        "MOV X1, XZR\n\t"
                        "MOV X2, XZR\n\t"
                        "MOV X3, XZR\n\t"
                        "MOV X4, XZR\n\t"
                        "MOV X5, XZR\n\t"
                        "MOV X8, #101\n\t" // __NR_nanosleep
                        "STR LR, [SP, #-16]!\n\t"
                        "MOV LR, SP\n\t"
                        "SVC #0\n\t"
                        "MOV SP, LR\n\t"
                        "LDR LR, [SP], #16" :: "r"(&spec) : "x0", "x1", "x2", "x3", "x4", "x5", "x8");
                }
            }
            return;
        } else if (svc == 0x1E) { // svcGetSystemTick
            asm("STP X1, X2, [SP, #-16]!\n\t"
                "STR Q0, [SP, #-16]!\n\t"
                "STR Q1, [SP, #-16]!\n\t"
                "STR Q2, [SP, #-16]!\n\t"
                "MRS X1, CNTFRQ_EL0\n\t"
                "MRS X2, CNTVCT_EL0\n\t"
                "UCVTF D0, X0\n\t"
                "MOV X1, 87411174408192\n\t"
                "MOVK X1, 0x4172, LSL 48\n\t"
                "FMOV D2, X1\n\t"
                "UCVTF D1, X1\n\t"
                "FDIV D0, D0, D2\n\t"
                "FMUL D0, D0, D1\n\t"
                "FCVTZU %0, D0\n\t"
                "LDR Q2, [SP], #16\n\t"
                "LDR Q1, [SP], #16\n\t"
                "LDR Q0, [SP], #16\n\t"
                "LDP X1, X2, [SP], #16" :: "r"(ctx->registers.x0));
            return;
        }
        while (true) {
            ctx->state = ThreadState::WaitKernel;
            while (ctx->state == ThreadState::WaitKernel);
            if (ctx->state == ThreadState::WaitRun)
                break;
            else if (ctx->state == ThreadState::WaitFunc) {
                if (ctx->commandId == static_cast<u32>(ThreadCall::Syscall)) {
                    saveCtxStack();
                    loadCtxTls();
                    asm("STR LR, [SP, #-16]!\n\t"
                        "MOV LR, SP\n\t"
                        "SVC #0\n\t"
                        "MOV SP, LR\n\t"
                        "LDR LR, [SP], #16");
                    saveCtxTls();
                    loadCtxStack();
                } else if (ctx->commandId == static_cast<u32>(ThreadCall::Memcopy)) {
                    auto src = reinterpret_cast<u8*>(ctx->registers.x0);
                    auto dest = reinterpret_cast<u8*>(ctx->registers.x1);
                    auto size = ctx->registers.x2;
                    auto end = src + size;
                    while (src < end)
                        *(src++) = *(dest++);
                } else if (ctx->commandId == static_cast<u32>(ThreadCall::Clone)) {
                    saveCtxStack();
                    loadCtxTls();
                    asm("STR LR, [SP, #-16]!\n\t"
                        "MOV LR, SP\n\t"
                        "SVC #0\n\t"
                        "CBNZ X0, .parent\n\t"
                        "MSR TPIDR_EL0, X3\n\t"
                        "MOV LR, 0\n\t"
                        "MOV X0, X6\n\t"
                        "MOV X1, XZR\n\t"
                        "MOV X2, XZR\n\t"
                        "MOV X3, XZR\n\t"
                        "MOV X4, XZR\n\t"
                        "MOV X6, XZR\n\t"
                        "MOV X7, XZR\n\t"
                        "MOV X8, XZR\n\t"
                        "MOV X9, XZR\n\t"
                        "MOV X10, XZR\n\t"
                        "MOV X11, XZR\n\t"
                        "MOV X12, XZR\n\t"
                        "MOV X13, XZR\n\t"
                        "MOV X14, XZR\n\t"
                        "MOV X15, XZR\n\t"
                        "MOV X16, XZR\n\t"
                        "MOV X17, XZR\n\t"
                        "MOV X18, XZR\n\t"
                        "MOV X19, XZR\n\t"
                        "MOV X20, XZR\n\t"
                        "MOV X21, XZR\n\t"
                        "MOV X22, XZR\n\t"
                        "MOV X23, XZR\n\t"
                        "MOV X24, XZR\n\t"
                        "MOV X25, XZR\n\t"
                        "MOV X26, XZR\n\t"
                        "MOV X27, XZR\n\t"
                        "MOV X28, XZR\n\t"
                        "MOV X29, XZR\n\t"
                        "BR X5\n\t"
                        ".parent:\n\t"
                        "MOV SP, LR\n\t"
                        "LDR LR, [SP], #16");
                    saveCtxTls();
                    loadCtxStack();
                }
            }
        }
        ctx->state = ThreadState::Running;
    }

    void signalHandler(int signal, siginfo_t *info, ucontext_t *ucontext) {
        volatile ThreadContext *ctx;
        asm("MRS %0, TPIDR_EL0":"=r"(ctx));
        for (u8 index = 0; index < 30; index++)
            ctx->registers.regs[index] = ucontext->uc_mcontext.regs[index];
        ctx->pc = ucontext->uc_mcontext.pc;
        ctx->commandId = static_cast<u32>(signal);
        ctx->faultAddress = ucontext->uc_mcontext.fault_address;
        ctx->sp = ucontext->uc_mcontext.sp;
        while (true) {
            ctx->state = ThreadState::GuestCrash;
            if (ctx->state == ThreadState::WaitRun)
                exit(0);
        }
    }

    void entry(u64 address) {
        volatile ThreadContext *ctx;
        asm("MRS %0, TPIDR_EL0":"=r"(ctx));
        while (true) {
            ctx->state = ThreadState::WaitInit;
            while (ctx->state == ThreadState::WaitInit);
            if (ctx->state == ThreadState::WaitRun)
                break;
            else if (ctx->state == ThreadState::WaitFunc) {
                if (ctx->commandId == static_cast<u32>(ThreadCall::Syscall)) {
                    saveCtxStack();
                    loadCtxTls();
                    asm("STR LR, [SP, #-16]!\n\t"
                        "MOV LR, SP\n\t"
                        "SVC #0\n\t"
                        "MOV SP, LR\n\t"
                        "LDR LR, [SP], #16");
                    saveCtxTls();
                    loadCtxStack();
                }
            } else if (ctx->commandId == static_cast<u32>(ThreadCall::Memcopy)) {
                auto src = reinterpret_cast<u8*>(ctx->registers.x0);
                auto dest = reinterpret_cast<u8*>(ctx->registers.x1);
                auto size = ctx->registers.x2;
                auto end = src + size;
                while (src < end)
                    *(src++) = *(dest++);
            }
        }
        struct sigaction sigact{
            .sa_sigaction = reinterpret_cast<void (*)(int, struct siginfo *, void *)>(reinterpret_cast<void *>(signalHandler)),
            .sa_flags = SA_SIGINFO,
        };
        /*
        for (int signal : {SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV})
            sigaction(signal, &sigact, nullptr);
        */
        ctx->state = ThreadState::Running;
        asm("MOV LR, %0\n\t"
            "MOV X0, %1\n\t"
            "MOV X1, %2\n\t"
            "MOV X2, XZR\n\t"
            "MOV X3, XZR\n\t"
            "MOV X4, XZR\n\t"
            "MOV X5, XZR\n\t"
            "MOV X6, XZR\n\t"
            "MOV X7, XZR\n\t"
            "MOV X8, XZR\n\t"
            "MOV X9, XZR\n\t"
            "MOV X10, XZR\n\t"
            "MOV X11, XZR\n\t"
            "MOV X12, XZR\n\t"
            "MOV X13, XZR\n\t"
            "MOV X14, XZR\n\t"
            "MOV X15, XZR\n\t"
            "MOV X16, XZR\n\t"
            "MOV X17, XZR\n\t"
            "MOV X18, XZR\n\t"
            "MOV X19, XZR\n\t"
            "MOV X20, XZR\n\t"
            "MOV X21, XZR\n\t"
            "MOV X22, XZR\n\t"
            "MOV X23, XZR\n\t"
            "MOV X24, XZR\n\t"
            "MOV X25, XZR\n\t"
            "MOV X26, XZR\n\t"
            "MOV X27, XZR\n\t"
            "MOV X28, XZR\n\t"
            "MOV X29, XZR\n\t"
            "RET" :: "r"(address), "r"(ctx->registers.x0), "r"(ctx->registers.x1) : "x0", "x1", "lr");
        __builtin_unreachable();
    }
}
