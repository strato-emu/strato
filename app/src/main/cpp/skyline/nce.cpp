// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <cxxabi.h>
#include <unistd.h>
#include "common/signal.h"
#include "common/trace.h"
#include "os.h"
#include "jvm.h"
#include "kernel/types/KProcess.h"
#include "kernel/svc.h"
#include "nce/guest.h"
#include "nce/instructions.h"
#include "nce.h"

namespace skyline::nce {
    void NCE::SvcHandler(u16 svcId, ThreadContext *ctx) {
        TRACE_EVENT_END("guest");

        const auto &state{*ctx->state};
        auto svc{kernel::svc::SvcTable[svcId]};
        try {
            if (svc) [[likely]] {
                TRACE_EVENT("kernel", perfetto::StaticString{svc.name});
                (svc.function)(state);
            } else {
                throw exception("Unimplemented SVC 0x{:X}", svcId);
            }

            while (kernel::Scheduler::YieldPending) [[unlikely]] {
                state.scheduler->Rotate(false);
                kernel::Scheduler::YieldPending = false;
                state.scheduler->WaitSchedule();
            }
        } catch (const signal::SignalException &e) {
            if (e.signal != SIGINT) {
                state.logger->ErrorNoPrefix("{} (SVC: {})\nStack Trace:{}", e.what(), svc.name, state.loader->GetStackTrace(e.frames));
                if (state.thread->id) {
                    signal::BlockSignal({SIGINT});
                    state.process->Kill(false);
                }
            }
            abi::__cxa_end_catch(); // We call this prior to the longjmp to cause the exception object to be destroyed
            std::longjmp(state.thread->originalCtx, true);
        } catch (const std::exception &e) {
            if (svc)
                state.logger->ErrorNoPrefix("{} (SVC: {})\nStack Trace:{}", e.what(), svc.name, state.loader->GetStackTrace());
            else
                state.logger->ErrorNoPrefix("{} (SVC: 0x{:X})\nStack Trace:{}", e.what(), svcId, state.loader->GetStackTrace());

            if (state.thread->id) {
                signal::BlockSignal({SIGINT});
                state.process->Kill(false);
            }
            abi::__cxa_end_catch();
            std::longjmp(state.thread->originalCtx, true);
        }

        TRACE_EVENT_BEGIN("guest", "Guest");
    }

    void NCE::SignalHandler(int signal, siginfo *info, ucontext *ctx, void **tls) {
        if (*tls) { // If TLS was restored then this occurred in guest code
            auto &mctx{ctx->uc_mcontext};
            const auto &state{*reinterpret_cast<ThreadContext *>(*tls)->state};
            if (signal != SIGINT) {
                signal::StackFrame topFrame{.lr = reinterpret_cast<void *>(ctx->uc_mcontext.pc), .next = reinterpret_cast<signal::StackFrame *>(ctx->uc_mcontext.regs[29])};
                std::string trace{state.loader->GetStackTrace(&topFrame)};

                std::string cpuContext;
                if (mctx.fault_address)
                    cpuContext += fmt::format("\n  Fault Address: 0x{:X}", mctx.fault_address);
                if (mctx.sp)
                    cpuContext += fmt::format("\n  Stack Pointer: 0x{:X}", mctx.sp);
                for (u8 index{}; index < (sizeof(mcontext_t::regs) / sizeof(u64)); index += 2)
                    cpuContext += fmt::format("\n  X{:<2}: 0x{:<16X} X{:<2}: 0x{:X}", index, mctx.regs[index], index + 1, mctx.regs[index + 1]);

                state.logger->Error("Thread #{} has crashed due to signal: {}\nStack Trace:{}\nCPU Context:{}", state.thread->id, strsignal(signal), trace, cpuContext);

                if (state.thread->id) {
                    signal::BlockSignal({SIGINT});
                    state.process->Kill(false);
                }
            }

            mctx.pc = reinterpret_cast<u64>(&std::longjmp);
            mctx.regs[0] = reinterpret_cast<u64>(state.thread->originalCtx);
            mctx.regs[1] = true;

            *tls = nullptr;
        } else { // If TLS wasn't restored then this occurred in host code
            signal::ExceptionalSignalHandler(signal, info, ctx); //!< Delegate throwing a host exception to the exceptional signal handler
        }
    }

    void *NceTlsRestorer() {
        ThreadContext *threadCtx;
        asm volatile("MRS %x0, TPIDR_EL0":"=r"(threadCtx));
        if (threadCtx->magic != constant::SkyTlsMagic)
            return nullptr;
        asm volatile("MSR TPIDR_EL0, %x0"::"r"(threadCtx->hostTpidrEl0));
        return threadCtx;
    }

    NCE::NCE(const DeviceState &state) : state(state) {
        signal::SetTlsRestorer(&NceTlsRestorer);
    }

    constexpr u8 MainSvcTrampolineSize{17}; // Size of the main SVC trampoline function in u32 units
    constexpr u32 TpidrEl0{0x5E82};         // ID of TPIDR_EL0 in MRS
    constexpr u32 TpidrroEl0{0x5E83};       // ID of TPIDRRO_EL0 in MRS
    constexpr u32 CntfrqEl0{0x5F00};        // ID of CNTFRQ_EL0 in MRS
    constexpr u32 CntpctEl0{0x5F01};        // ID of CNTPCT_EL0 in MRS
    constexpr u32 CntvctEl0{0x5F02};        // ID of CNTVCT_EL0 in MRS
    constexpr u32 TegraX1Freq{19200000};    // The clock frequency of the Tegra X1 (19.2 MHz)

    NCE::PatchData NCE::GetPatchData(const std::vector<u8> &text) {
        size_t size{guest::SaveCtxSize + guest::LoadCtxSize + MainSvcTrampolineSize};
        std::vector<size_t> offsets;

        u64 frequency;
        asm("MRS %0, CNTFRQ_EL0" : "=r"(frequency));
        bool rescaleClock{frequency != TegraX1Freq};

        auto start{reinterpret_cast<const u32 *>(text.data())}, end{reinterpret_cast<const u32 *>(text.data() + text.size())};
        for (const u32 *instruction{start}; instruction < end; instruction++) {
            auto svc{*reinterpret_cast<const instructions::Svc *>(instruction)};
            auto mrs{*reinterpret_cast<const instructions::Mrs *>(instruction)};
            auto msr{*reinterpret_cast<const instructions::Msr *>(instruction)};

            if (svc.Verify()) {
                size += 7;
                offsets.push_back(instruction - start);
            } else if (mrs.Verify()) {
                if (mrs.srcReg == TpidrroEl0 || mrs.srcReg == TpidrEl0) {
                    size += ((mrs.destReg != registers::X0) ? 6 : 3);
                    offsets.push_back(instruction - start);
                } else {
                    if (rescaleClock) {
                        if (mrs.srcReg == CntpctEl0) {
                            size += guest::RescaleClockSize + 3;
                            offsets.push_back(instruction - start);
                        } else if (mrs.srcReg == CntfrqEl0) {
                            size += 3;
                            offsets.push_back(instruction - start);
                        }
                    } else if (mrs.srcReg == CntpctEl0) {
                        offsets.push_back(instruction - start);
                    }
                }
            } else if (msr.Verify() && msr.destReg == TpidrEl0) {
                size += 6;
                offsets.push_back(instruction - start);
            }
        }
        return {util::AlignUp(size * sizeof(u32), PAGE_SIZE), offsets};
    }

    void NCE::PatchCode(std::vector<u8> &text, u32 *patch, size_t patchSize, const std::vector<size_t> &offsets) {
        u32 *start{patch};
        u32 *end{patch + (patchSize / sizeof(u32))};

        std::memcpy(patch, reinterpret_cast<void *>(&guest::SaveCtx), guest::SaveCtxSize * sizeof(u32));
        patch += guest::SaveCtxSize;

        {
            /* Main SVC Trampoline */
            /* Store LR in 16B of pre-allocated stack */
            *patch++ = 0xF90007FE; // STR LR, [SP, #8]

            /* Replace Skyline TLS with host TLS */
            *patch++ = 0xD53BD041; // MRS X1, TPIDR_EL0
            *patch++ = 0xF9415022; // LDR X2, [X1, #0x2A0] (ThreadContext::hostTpidrEl0)
            *patch++ = 0xD51BD042; // MSR TPIDR_EL0, X2

            /* Replace guest stack with host stack */
            *patch++ = 0x910003E2; // MOV X2, SP
            *patch++ = 0xF9415423; // LDR X3, [X1, #0x2A8] (ThreadContext::hostSp)
            *patch++ = 0x9100007F; // MOV SP, X3

            /* Store Skyline TLS + guest SP on stack */
            *patch++ = 0xA9BF0BE1; // STP X1, X2, [SP, #-16]!

            /* Jump to SvcHandler */
            for (const auto &mov : instructions::MoveRegister(registers::X2, reinterpret_cast<u64>(&NCE::SvcHandler)))
                if (mov)
                    *patch++ = mov;
            *patch++ = 0xD63F0040; // BLR X2

            /* Restore Skyline TLS + guest SP */
            *patch++ = 0xA8C10BE1; // LDP X1, X2, [SP], #16
            *patch++ = 0xD51BD041; // MSR TPIDR_EL0, X1
            *patch++ = 0x9100005F; // MOV SP, X2

            /* Restore LR and Return */
            *patch++ = 0xF94007FE; // LDR LR, [SP, #8]
            *patch++ = 0xD65F03C0; // RET
        }

        std::memcpy(patch, reinterpret_cast<void *>(&guest::LoadCtx), guest::LoadCtxSize * sizeof(u32));
        patch += guest::LoadCtxSize;

        u64 frequency;
        asm("MRS %0, CNTFRQ_EL0" : "=r"(frequency));
        bool rescaleClock{frequency != TegraX1Freq};

        for (auto offset : offsets) {
            u32 *instruction{reinterpret_cast<u32 *>(text.data()) + offset};
            auto svc{*reinterpret_cast<instructions::Svc *>(instruction)};
            auto mrs{*reinterpret_cast<instructions::Mrs *>(instruction)};
            auto msr{*reinterpret_cast<instructions::Msr *>(instruction)};

            if (svc.Verify()) {
                /* Per-SVC Trampoline */
                /* Rewrite SVC with B to trampoline */
                *instruction = instructions::B((end - patch) + offset, true).raw;

                /* Save Context */
                *patch++ = 0xF81F0FFE; // STR LR, [SP, #-16]!
                *patch = instructions::BL(start - patch).raw;
                patch++;

                /* Jump to main SVC trampoline */
                *patch++ = instructions::Movz(registers::W0, static_cast<u16>(svc.value)).raw;
                *patch = instructions::BL((start - patch) + guest::SaveCtxSize).raw;
                patch++;

                /* Restore Context and Return */
                *patch = instructions::BL((start - patch) + guest::SaveCtxSize + MainSvcTrampolineSize).raw;
                patch++;
                *patch++ = 0xF84107FE; // LDR LR, [SP], #16
                *patch = instructions::B((end - patch) + offset + 1).raw;
                patch++;
            } else if (mrs.Verify()) {
                if (mrs.srcReg == TpidrroEl0 || mrs.srcReg == TpidrEl0) {
                    /* Emulated TLS Register Load */
                    /* Rewrite MRS with B to trampoline */
                    *instruction = instructions::B((end - patch) + offset, true).raw;

                    /* Allocate Scratch Register */
                    if (mrs.destReg != registers::X0)
                        *patch++ = 0xF81F0FE0; // STR X0, [SP, #-16]!

                    /* Retrieve emulated TLS register from ThreadContext */
                    *patch++ = 0xD53BD040; // MRS X0, TPIDR_EL0
                    if (mrs.srcReg == TpidrroEl0)
                        *patch++ = 0xF9415800; // LDR X0, [X0, #0x2B0] (ThreadContext::tpidrroEl0)
                    else
                        *patch++ = 0xF9415C00; // LDR X0, [X0, #0x2B8] (ThreadContext::tpidrEl0)

                    /* Restore Scratch Register and Return */
                    if (mrs.destReg != registers::X0) {
                        *patch++ = instructions::Mov(registers::X(mrs.destReg), registers::X0).raw;
                        *patch++ = 0xF84107E0; // LDR X0, [SP], #16
                    }
                    *patch = instructions::B((end - patch) + offset + 1).raw;
                    patch++;
                } else {
                    if (rescaleClock) {
                        if (mrs.srcReg == CntpctEl0) {
                            /* Physical Counter Load Emulation (With Rescaling) */
                            /* Rewrite MRS with B to trampoline */
                            *instruction = instructions::B((end - patch) + offset, true).raw;

                            /* Rescale host clock */
                            std::memcpy(patch, reinterpret_cast<void *>(&guest::RescaleClock), guest::RescaleClockSize * sizeof(u32));
                            patch += guest::RescaleClockSize;

                            /* Load result from stack into destination register */
                            instructions::Ldr ldr(0xF94003E0); // LDR XOUT, [SP]
                            ldr.destReg = mrs.destReg;
                            *patch++ = ldr.raw;

                            /* Free 32B stack allocation by RescaleClock and Return */
                            *patch++ = {0x910083FF}; // ADD SP, SP, #32
                            *patch = instructions::B((end - patch) + offset + 1).raw;
                            patch++;
                        } else if (mrs.srcReg == CntfrqEl0) {
                            /* Physical Counter Frequency Load Emulation */
                            /* Rewrite MRS with B to trampoline */
                            *instruction = instructions::B((end - patch) + offset, true).raw;

                            /* Write back Tegra X1 Counter Frequency and Return */
                            for (const auto &mov : instructions::MoveRegister(registers::X(mrs.destReg), TegraX1Freq))
                                *patch++ = mov;
                            *patch = instructions::B((end - patch) + offset + 1).raw;
                            patch++;
                        }
                    } else if (mrs.srcReg == CntpctEl0) {
                        /* Physical Counter Load Emulation (Without Rescaling) */
                        // We just convert CNTPCT_EL0 -> CNTVCT_EL0 as Linux doesn't allow access to the physical counter
                        *instruction = instructions::Mrs(CntvctEl0, registers::X(mrs.destReg)).raw;
                    }
                }
            } else if (msr.Verify() && msr.destReg == TpidrEl0) {
                /* Emulated TLS Register Store */
                /* Rewrite MSR with B to trampoline */
                *instruction = instructions::B((end - patch) + offset, true).raw;

                /* Allocate Scratch Registers */
                bool x0x1{mrs.srcReg != registers::X0 && mrs.srcReg != registers::X1};
                *patch++ = x0x1 ? 0xA9BF07E0 : 0xA9BF0FE2; // STP X(0/2), X(1/3), [SP, #-16]!

                /* Store new TLS value into ThreadContext */
                *patch++ = x0x1 ? 0xD53BD040 : 0xD53BD042; // MRS X(0/2), TPIDR_EL0
                *patch++ = instructions::Mov(x0x1 ? registers::X1 : registers::X3, registers::X(msr.srcReg)).raw;
                *patch++ = x0x1 ? 0xF9015C01 : 0xF9015C03; // STR X(1/3), [X0, #0x4B8] (ThreadContext::tpidrEl0)

                /* Restore Scratch Registers and Return */
                *patch++ = x0x1 ? 0xA8C107E0 : 0xA8C10FE2; // LDP X(0/2), X(1/3), [SP], #16
                *patch = instructions::B((end - patch) + offset + 1).raw;
                patch++;
            }
        }
    }
}
