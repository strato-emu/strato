// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <sched.h>
#include <unistd.h>
#include "os.h"
#include "gpu.h"
#include "jvm.h"
#include "kernel/types/KProcess.h"
#include "kernel/svc.h"
#include "nce/guest.h"
#include "nce/instructions.h"
#include "nce.h"

extern bool Halt;
extern jobject Surface;
extern skyline::GroupMutex JniMtx;

namespace skyline {
    void NCE::KernelThread(pid_t thread) {
        /*
        state.jvm->AttachThread();
        try {
            state.thread = state.process->threads.at(thread);
            state.ctx = reinterpret_cast<ThreadContext *>(state.thread->ctxMemory->kernel.ptr);

            while (true) {
                asm("yield");

                if (__predict_false(Halt))
                    break;
                if (__predict_false(!Surface))
                    continue;

                if (state.ctx->state == ThreadState::WaitKernel) {
                    std::lock_guard jniGd(JniMtx);

                    if (__predict_false(Halt))
                        break;
                    if (__predict_false(!Surface))
                        continue;

                    auto svc{state.ctx->svc};

                    try {
                        if (kernel::svc::SvcTable[svc]) {
                            state.logger->Debug("SVC called 0x{:X}", svc);
                            (*kernel::svc::SvcTable[svc])(state);
                        } else {
                            throw exception("Unimplemented SVC 0x{:X}", svc);
                        }
                    } catch (const std::exception &e) {
                        throw exception("{} (SVC: 0x{:X})", e.what(), svc);
                    }

                    state.ctx->state = ThreadState::WaitRun;
                } else if (__predict_false(state.ctx->state == ThreadState::GuestCrash)) {
                    state.logger->Warn("Thread with PID {} has crashed due to signal: {}", thread, strsignal(state.ctx->svc));
                    ThreadTrace();

                    state.ctx->state = ThreadState::WaitRun;
                    break;
                }
            }
        } catch (const std::exception &e) {
            state.logger->Error(e.what());
        } catch (...) {
            state.logger->Error("An unknown exception has occurred");
        }

        if (!Halt) {
            if (thread == state.process->pid) {
                JniMtx.lock(GroupMutex::Group::Group2);

                state.os->KillThread(thread);
                Halt = true;

                JniMtx.unlock();
            } else {
                state.os->KillThread(thread);
            }
        }

        state.jvm->DetachThread();
         */
    }

    NCE::NCE(DeviceState &state) : state(state) {}

    NCE::~NCE() {
        for (auto &thread : threadMap)
            thread.second->join();
    }

    void NCE::Execute() {
        try {
            while (true) {
                std::lock_guard guard(JniMtx);
                if (Halt)
                    break;
                state.gpu->Loop();
            }
        } catch (const std::exception &e) {
            state.logger->Error(e.what());
        } catch (...) {
            state.logger->Error("An unknown exception has occurred");
        }

        if (!Halt) {
            JniMtx.lock(GroupMutex::Group::Group2);
            Halt = true;
            JniMtx.unlock();
        }
    }

    inline ThreadContext *GetContext() {
        ThreadContext *ctx;
        asm("MRS %0, TPIDR_EL0":"=r"(ctx));
        return ctx;
    }

    void NCE::SignalHandler(int signal, const siginfo &info, const ucontext &context) {

    }

    void NCE::SignalHandler(int signal, siginfo *info, void *context) {
        (GetContext()->nce)->SignalHandler(signal, *info, *reinterpret_cast<ucontext*>(context));
    }

    void NCE::ThreadTrace(u16 instructionCount, ThreadContext *ctx) {
        std::string raw;
        std::string trace;
        std::string regStr;

        ctx = ctx ? ctx : state.ctx;

        if (instructionCount) {
            auto offset{ctx->pc - (instructionCount * sizeof(u32)) + (2 * sizeof(u32))};
            span instructions(reinterpret_cast<u32 *>(offset), instructionCount);

            for (auto &instruction : instructions) {
                instruction = __builtin_bswap32(instruction);

                if (offset == ctx->pc)
                    trace += fmt::format("\n-> 0x{:X} : 0x{:08X}", offset, instruction);
                else
                    trace += fmt::format("\n   0x{:X} : 0x{:08X}", offset, instruction);

                raw += fmt::format("{:08X}", instruction);
                offset += sizeof(u32);
            }
        }

        //if (ctx->faultAddress)
        //    regStr += fmt::format("\nFault Address: 0x{:X}", ctx->faultAddress);

        if (ctx->sp)
            regStr += fmt::format("\nStack Pointer: 0x{:X}", ctx->sp);

        constexpr u8 numRegisters{31}; //!< The amount of general-purpose registers in ARMv8
        for (u8 index{}; index < numRegisters - 2; index += 2) {
            auto xStr{index < 10 ? " X" : "X"};
            regStr += fmt::format("\n{}{}: 0x{:<16X} {}{}: 0x{:X}", xStr, index, ctx->registers.regs[index], xStr, index + 1, ctx->registers.regs[index + 1]);
        }

        if (instructionCount) {
            state.logger->Debug("Process Trace:{}", trace);
            state.logger->Debug("Raw Instructions: 0x{}", raw);
            state.logger->Debug("CPU Context:{}", regStr);
        } else {
            state.logger->Debug("CPU Context:{}", regStr);
        }
    }

    std::vector<u32> NCE::PatchCode(std::vector<u8> &code, u64 baseAddress, i64 offset) {
        constexpr u32 TpidrEl0{0x5E82};      // ID of TPIDR_EL0 in MRS
        constexpr u32 TpidrroEl0{0x5E83};    // ID of TPIDRRO_EL0 in MRS
        constexpr u32 CntfrqEl0{0x5F00};     // ID of CNTFRQ_EL0 in MRS
        constexpr u32 CntpctEl0{0x5F01};     // ID of CNTPCT_EL0 in MRS
        constexpr u32 CntvctEl0{0x5F02};     // ID of CNTVCT_EL0 in MRS
        constexpr u32 TegraX1Freq{19200000}; // The clock frequency of the Tegra X1 (19.2 MHz)

        u32 *start{reinterpret_cast<u32 *>(code.data())};
        u32 *end{start + (code.size() / sizeof(u32))};
        i64 patchOffset{offset};

        std::vector<u32> patch((guest::SaveCtxSize + guest::LoadCtxSize + guest::SvcHandlerSize) / sizeof(u32));

        std::memcpy(patch.data(), reinterpret_cast<void *>(&guest::SaveCtx), guest::SaveCtxSize);
        offset += guest::SaveCtxSize;

        std::memcpy(reinterpret_cast<u8 *>(patch.data()) + guest::SaveCtxSize, reinterpret_cast<void *>(&guest::LoadCtx), guest::LoadCtxSize);
        offset += guest::LoadCtxSize;

        std::memcpy(reinterpret_cast<u8 *>(patch.data()) + guest::SaveCtxSize + guest::LoadCtxSize, reinterpret_cast<void *>(&guest::SvcHandler), guest::SvcHandlerSize);
        offset += guest::SvcHandlerSize;

        static u64 frequency{};
        if (!frequency)
            asm("MRS %0, CNTFRQ_EL0" : "=r"(frequency));

        for (u32 *address{start}; address < end; address++) {
            auto instrSvc{reinterpret_cast<instr::Svc *>(address)};
            auto instrMrs{reinterpret_cast<instr::Mrs *>(address)};
            auto instrMsr{reinterpret_cast<instr::Msr *>(address)};

            if (instrSvc->Verify()) {
                // If this is an SVC we need to branch to saveCtx then to the SVC Handler after putting the PC + SVC into X0 and W1 and finally loadCtx before returning to where we were before
                instr::B bJunc(offset);

                constexpr u32 strLr{0xF81F0FFE}; // STR LR, [SP, #-16]!
                offset += sizeof(strLr);

                instr::BL bSvCtx(patchOffset - offset);
                offset += sizeof(bSvCtx);

                auto movPc{instr::MoveRegister<u64>(regs::X0, baseAddress + (address - start))};
                offset += sizeof(u32) * movPc.size();

                instr::Movz movCmd(regs::W1, static_cast<u16>(instrSvc->value));
                offset += sizeof(movCmd);

                instr::BL bSvcHandler((patchOffset + guest::SaveCtxSize + guest::LoadCtxSize) - offset);
                offset += sizeof(bSvcHandler);

                instr::BL bLdCtx((patchOffset + guest::SaveCtxSize) - offset);
                offset += sizeof(bLdCtx);

                constexpr u32 ldrLr{0xF84107FE}; // LDR LR, [SP], #16
                offset += sizeof(ldrLr);

                instr::B bret(-offset + sizeof(u32));
                offset += sizeof(bret);

                *address = bJunc.raw;
                patch.push_back(strLr);
                patch.push_back(bSvCtx.raw);
                for (auto &instr : movPc)
                    patch.push_back(instr);
                patch.push_back(movCmd.raw);
                patch.push_back(bSvcHandler.raw);
                patch.push_back(bLdCtx.raw);
                patch.push_back(ldrLr);
                patch.push_back(bret.raw);
            } else if (instrMrs->Verify()) {
                if (instrMrs->srcReg == TpidrroEl0 || instrMrs->srcReg == TpidrEl0) {
                    // If this moves TPIDR(RO)_EL0 into a register then we retrieve the value of our virtual TPIDR(RO)_EL0 from TLS and write it to the register
                    instr::B bJunc(offset);

                    u32 strX0{};
                    if (instrMrs->destReg != regs::X0) {
                        strX0 = 0xF81F0FE0; // STR X0, [SP, #-16]!
                        offset += sizeof(strX0);
                    }

                    constexpr u32 mrsX0{0xD53BD040}; // MRS X0, TPIDR_EL0
                    offset += sizeof(mrsX0);

                    u32 ldrTls;
                    if (instrMrs->srcReg == TpidrroEl0)
                        ldrTls = 0xF9408000; // LDR X0, [X0, #256] (ThreadContext::tpidrroEl0)
                    else
                        ldrTls = 0xF9408400; // LDR X0, [X0, #264] (ThreadContext::tpidrEl0)

                    offset += sizeof(ldrTls);

                    u32 movXn{};
                    u32 ldrX0{};
                    if (instrMrs->destReg != regs::X0) {
                        movXn = instr::Mov(regs::X(instrMrs->destReg), regs::X0).raw;
                        offset += sizeof(movXn);

                        ldrX0 = 0xF84107E0; // LDR X0, [SP], #16
                        offset += sizeof(ldrX0);
                    }

                    instr::B bret(-offset + sizeof(u32));
                    offset += sizeof(bret);

                    *address = bJunc.raw;
                    if (strX0)
                        patch.push_back(strX0);
                    patch.push_back(mrsX0);
                    patch.push_back(ldrTls);
                    if (movXn)
                        patch.push_back(movXn);
                    if (ldrX0)
                        patch.push_back(ldrX0);
                    patch.push_back(bret.raw);
                } else if (frequency != TegraX1Freq) {
                    // These deal with changing the timer registers, we only do this if the clock frequency doesn't match the X1's clock frequency
                    if (instrMrs->srcReg == CntpctEl0) {
                        // If this moves CNTPCT_EL0 into a register then call RescaleClock to rescale the device's clock to the X1's clock frequency and write result to register
                        instr::B bJunc(offset);
                        offset += guest::RescaleClockSize;

                        instr::Ldr ldr(0xF94003E0); // LDR XOUT, [SP]
                        ldr.destReg = instrMrs->destReg;
                        offset += sizeof(ldr);

                        constexpr u32 addSp{0x910083FF}; // ADD SP, SP, #32
                        offset += sizeof(addSp);

                        instr::B bret(-offset + sizeof(u32));
                        offset += sizeof(bret);

                        *address = bJunc.raw;
                        auto size{patch.size()};
                        patch.resize(size + (guest::RescaleClockSize / sizeof(u32)));
                        std::memcpy(patch.data() + size, reinterpret_cast<void *>(&guest::RescaleClock), guest::RescaleClockSize);
                        patch.push_back(ldr.raw);
                        patch.push_back(addSp);
                        patch.push_back(bret.raw);
                    } else if (instrMrs->srcReg == CntfrqEl0) {
                        // If this moves CNTFRQ_EL0 into a register then move the Tegra X1's clock frequency into the register (Rather than the host clock frequency)
                        instr::B bJunc(offset);

                        auto movFreq{instr::MoveRegister<u32>(static_cast<regs::X>(instrMrs->destReg), TegraX1Freq)};
                        offset += sizeof(u32) * movFreq.size();

                        instr::B bret(-offset + sizeof(u32));
                        offset += sizeof(bret);

                        *address = bJunc.raw;
                        for (auto &instr : movFreq)
                            patch.push_back(instr);
                        patch.push_back(bret.raw);
                    }
                } else {
                    // If the host clock frequency is the same as the Tegra X1's clock frequency
                    if (instrMrs->srcReg == CntpctEl0) {
                        // If this moves CNTPCT_EL0 into a register, change the instruction to move CNTVCT_EL0 instead as Linux or most other OSes don't allow access to CNTPCT_EL0 rather only CNTVCT_EL0 can be accessed from userspace
                        *address = instr::Mrs(CntvctEl0, regs::X(instrMrs->destReg)).raw;
                    }
                }
            } else if (instrMsr->Verify()) {
                if (instrMsr->destReg == TpidrEl0) {
                    // If this moves a register into TPIDR_EL0 then we retrieve the value of the register and write it to our virtual TPIDR_EL0 in TLS
                    instr::B bJunc(offset);

                    // Used to avoid conflicts as we cannot read the source register from the stack
                    bool x0x1{instrMrs->srcReg != regs::X0 && instrMrs->srcReg != regs::X1};

                    // Push two registers to stack that can be used to load the TLS and arguments into
                    u32 pushXn{x0x1 ? 0xA9BF07E0 : 0xA9BF0FE2}; // STP X(0/2), X(1/3), [SP, #-16]!
                    offset += sizeof(pushXn);

                    u32 loadRealTls{x0x1 ? 0xD53BD040 : 0xD53BD042}; // MRS X(0/2), TPIDR_EL0
                    offset += sizeof(loadRealTls);

                    instr::Mov moveParam(x0x1 ? regs::X1 : regs::X3, regs::X(instrMsr->srcReg));
                    offset += sizeof(moveParam);

                    u32 storeEmuTls{x0x1 ? 0xF9008401 : 0xF9008403}; // STR X(1/3), [X0, #264] (ThreadContext::tpidrEl0)
                    offset += sizeof(storeEmuTls);

                    u32 popXn{x0x1 ? 0xA8C107E0 : 0xA8C10FE2}; // LDP X(0/2), X(1/3), [SP], #16
                    offset += sizeof(popXn);

                    instr::B bret(-offset + sizeof(u32));
                    offset += sizeof(bret);

                    *address = bJunc.raw;
                    patch.push_back(pushXn);
                    patch.push_back(loadRealTls);
                    patch.push_back(moveParam.raw);
                    patch.push_back(storeEmuTls);
                    patch.push_back(popXn);
                    patch.push_back(bret.raw);
                }
            }

            offset -= sizeof(u32);
            patchOffset -= sizeof(u32);
        }
        return patch;
    }
}
