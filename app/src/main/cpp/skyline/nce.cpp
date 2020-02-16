#include <sched.h>
#include <unistd.h>
#include "os.h"
#include "jvm.h"
#include "nce/guest.h"
#include "nce/instr.h"
#include "kernel/svc.h"
#include "nce.h"

extern bool Halt;
extern jobject Surface;
extern skyline::GroupMutex jniMtx;

namespace skyline {
    void NCE::KernelThread(pid_t thread) {
        try {
            state.thread = state.process->threads.at(thread);
            state.ctx = reinterpret_cast<ThreadContext *>(state.thread->ctxMemory->kernel.address);
            while (true) {
                asm("yield");
                if (Halt)
                    break;
                if (!Surface)
                    continue;
                if (state.ctx->state == ThreadState::WaitKernel) {
                    std::lock_guard jniGd(jniMtx);
                    if (Halt)
                        break;
                    if (!Surface)
                        continue;
                    const u16 svc = static_cast<const u16>(state.ctx->commandId);
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
                } else if (state.ctx->state == ThreadState::GuestCrash) {
                    state.logger->Warn("Thread with PID {} has crashed due to signal: {}", thread, strsignal(state.ctx->commandId));
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
                jniMtx.lock(GroupMutex::Group::Group2);
                state.os->KillThread(thread);
                Halt = true;
                jniMtx.unlock();
            } else {
                state.os->KillThread(thread);
            }
        }
    }

    NCE::NCE(DeviceState &state) : state(state) {}

    NCE::~NCE() {
        for (auto &thread : threadMap)
            thread.second->join();
    }

    void NCE::Execute() {
        while (true) {
            std::lock_guard guard(jniMtx);
            if (Halt)
                break;
            state.gpu->Loop();
        }
        if (!Halt) {
            jniMtx.lock(GroupMutex::Group::Group2);
            Halt = true;
            jniMtx.unlock();
        }
    }

    /**
     * This function will not work if optimizations are enabled as ThreadContext isn't volatile
     * and due to that is not read on every iteration of the while loop.
     * However, making ThreadContext or parts of it volatile slows down the applications as a whole.
     * So, we opted to use the hacky solution and disable optimizations for this single function.
     */
    void ExecuteFunctionCtx(ThreadCall call, Registers &funcRegs, ThreadContext *ctx) __attribute__ ((optnone)) {
        ctx->commandId = static_cast<u32>(call);
        Registers registers = ctx->registers;
        while (ctx->state != ThreadState::WaitInit && ctx->state != ThreadState::WaitKernel);
        ctx->registers = funcRegs;
        ctx->state = ThreadState::WaitFunc;
        while (ctx->state != ThreadState::WaitInit && ctx->state != ThreadState::WaitKernel);
        funcRegs = ctx->registers;
        ctx->registers = registers;
    }

    void NCE::ExecuteFunction(ThreadCall call, Registers &funcRegs, std::shared_ptr<kernel::type::KThread> &thread) {
        ExecuteFunctionCtx(call, funcRegs, reinterpret_cast<ThreadContext *>(thread->ctxMemory->kernel.address));
    }

    void NCE::ExecuteFunction(ThreadCall call, Registers &funcRegs) {
        if (state.process->status == kernel::type::KProcess::Status::Exiting)
            throw exception("Executing function on Exiting process");
        auto thread = state.thread ? state.thread : state.process->threads.at(state.process->pid);
        ExecuteFunctionCtx(call, funcRegs, reinterpret_cast<ThreadContext *>(thread->ctxMemory->kernel.address));
    }

    void NCE::WaitThreadInit(std::shared_ptr<kernel::type::KThread> &thread) __attribute__ ((optnone)) {
        auto ctx = reinterpret_cast<ThreadContext *>(thread->ctxMemory->kernel.address);
        while (ctx->state == ThreadState::NotReady);
    }

    void NCE::StartThread(u64 entryArg, u32 handle, std::shared_ptr<kernel::type::KThread> &thread) {
        auto ctx = reinterpret_cast<ThreadContext *>(thread->ctxMemory->kernel.address);
        while (ctx->state != ThreadState::WaitInit);
        ctx->tpidrroEl0 = thread->tls;
        ctx->registers.x0 = entryArg;
        ctx->registers.x1 = handle;
        ctx->state = ThreadState::WaitRun;
        state.logger->Debug("Starting kernel thread for guest thread: {}", thread->pid);
        threadMap[thread->pid] = std::make_shared<std::thread>(&NCE::KernelThread, this, thread->pid);
    }

    void NCE::ThreadTrace(u16 numHist, ThreadContext *ctx) {
        std::string raw;
        std::string trace;
        std::string regStr;
        ctx = ctx ? ctx : state.ctx;
        if (numHist) {
            std::vector<u32> instrs(numHist);
            u64 size = sizeof(u32) * numHist;
            u64 offset = ctx->pc - size + (2 * sizeof(u32));
            state.process->ReadMemory(instrs.data(), offset, size);
            for (auto &instr : instrs) {
                instr = __builtin_bswap32(instr);
                if (offset == ctx->pc)
                    trace += fmt::format("\n-> 0x{:X} : 0x{:08X}", offset, instr);
                else
                    trace += fmt::format("\n   0x{:X} : 0x{:08X}", offset, instr);
                raw += fmt::format("{:08X}", instr);
                offset += sizeof(u32);
            }
        }
        if (ctx->faultAddress)
            regStr += fmt::format("\nFault Address: 0x{:X}", ctx->faultAddress);
        if (ctx->sp)
            regStr += fmt::format("\nStack Pointer: 0x{:X}", ctx->sp);
        for (u16 index = 0; index < constant::NumRegs - 1; index += 2) {
            auto xStr = index < 10 ? " X" : "X";
            regStr += fmt::format("\n{}{}: 0x{:<16X} {}{}: 0x{:X}", xStr, index, ctx->registers.regs[index], xStr, index + 1, ctx->registers.regs[index + 1]);
        }
        if (numHist) {
            state.logger->Debug("Process Trace:{}", trace);
            state.logger->Debug("Raw Instructions: 0x{}", raw);
            state.logger->Debug("CPU Context:{}", regStr);
        } else {
            state.logger->Debug("CPU Context:{}", regStr);
        }
    }

    std::vector<u32> NCE::PatchCode(std::vector<u8> &code, u64 baseAddress, i64 offset) {
        u32 *start = reinterpret_cast<u32 *>(code.data());
        u32 *end = start + (code.size() / sizeof(u32));
        i64 patchOffset = offset;

        std::vector<u32> patch((guest::saveCtxSize + guest::loadCtxSize + guest::svcHandlerSize) / sizeof(u32));

        std::memcpy(patch.data(), reinterpret_cast<void *>(&guest::SaveCtx), guest::saveCtxSize);
        offset += guest::saveCtxSize;

        std::memcpy(reinterpret_cast<u8 *>(patch.data()) + guest::saveCtxSize, reinterpret_cast<void *>(&guest::LoadCtx), guest::loadCtxSize);
        offset += guest::loadCtxSize;

        std::memcpy(reinterpret_cast<u8 *>(patch.data()) + guest::saveCtxSize + guest::loadCtxSize, reinterpret_cast<void *>(&guest::SvcHandler), guest::svcHandlerSize);
        offset += guest::svcHandlerSize;

        static u64 frequency{};
        if (!frequency)
            asm("MRS %0, CNTFRQ_EL0" : "=r"(frequency));

        for (u32 *address = start; address < end; address++) {
            auto instrSvc = reinterpret_cast<instr::Svc *>(address);
            auto instrMrs = reinterpret_cast<instr::Mrs *>(address);

            if (instrSvc->Verify()) {
                instr::B bjunc(offset);
                constexpr u32 strLr = 0xF81F0FFE; // STR LR, [SP, #-16]!
                offset += sizeof(strLr);
                instr::BL bSvCtx(patchOffset - offset);
                offset += sizeof(bSvCtx);

                auto movPc = instr::MoveU64Reg(regs::X0, baseAddress + (address - start));
                offset += sizeof(u32) * movPc.size();
                instr::Movz movCmd(regs::W1, static_cast<u16>(instrSvc->value));
                offset += sizeof(movCmd);
                instr::BL bSvcHandler((patchOffset + guest::saveCtxSize + guest::loadCtxSize) - offset);
                offset += sizeof(bSvcHandler);

                instr::BL bLdCtx((patchOffset + guest::saveCtxSize) - offset);
                offset += sizeof(bLdCtx);
                constexpr u32 ldrLr = 0xF84107FE; // LDR LR, [SP], #16
                offset += sizeof(ldrLr);
                instr::B bret(-offset + sizeof(u32));
                offset += sizeof(bret);

                *address = bjunc.raw;
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
                if (instrMrs->srcReg == constant::TpidrroEl0) {
                    instr::B bjunc(offset);
                    u32 strX0{};
                    if (instrMrs->destReg != regs::X0) {
                        strX0 = 0xF81F0FE0; // STR X0, [SP, #-16]!
                        offset += sizeof(strX0);
                    }
                    const u32 mrsX0 = 0xD53BD040; // MRS X0, TPIDR_EL0
                    offset += sizeof(mrsX0);
                    const u32 ldrTls = 0xF9408000; // LDR X0, [X0, #256]
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

                    *address = bjunc.raw;
                    if (strX0)
                        patch.push_back(strX0);
                    patch.push_back(mrsX0);
                    patch.push_back(ldrTls);
                    if (movXn)
                        patch.push_back(movXn);
                    if (ldrX0)
                        patch.push_back(ldrX0);
                    patch.push_back(bret.raw);
                } else if (frequency != constant::TegraX1Freq) {
                    if (instrMrs->srcReg == constant::CntpctEl0) {
                        instr::B bjunc(offset);
                        offset += guest::rescaleClockSize;
                        instr::Ldr ldr(0xF94003E0); // LDR XOUT, [SP]
                        ldr.destReg = instrMrs->destReg;
                        offset += sizeof(ldr);
                        const u32 addSp = 0x910083FF; // ADD SP, SP, #32
                        offset += sizeof(addSp);
                        instr::B bret(-offset + sizeof(u32));
                        offset += sizeof(bret);

                        *address = bjunc.raw;
                        auto size = patch.size();
                        patch.resize(size + (guest::rescaleClockSize / sizeof(u32)));
                        std::memcpy(patch.data() + size, reinterpret_cast<void *>(&guest::RescaleClock), guest::rescaleClockSize);
                        patch.push_back(ldr.raw);
                        patch.push_back(addSp);
                        patch.push_back(bret.raw);
                    } else if (instrMrs->srcReg == constant::CntfrqEl0) {
                        instr::B bjunc(offset);
                        auto movFreq = instr::MoveU32Reg(static_cast<regs::X>(instrMrs->destReg), constant::TegraX1Freq);
                        offset += sizeof(u32) * movFreq.size();
                        instr::B bret(-offset + sizeof(u32));
                        offset += sizeof(bret);

                        *address = bjunc.raw;
                        for (auto &instr : movFreq)
                            patch.push_back(instr);
                        patch.push_back(bret.raw);
                    }
                } else {
                    if (instrMrs->srcReg == constant::CntpctEl0) {
                        instr::Mrs mrs(constant::CntvctEl0, regs::X(instrMrs->destReg));
                        *address = mrs.raw;
                    }
                }
            }
            offset -= sizeof(u32);
            patchOffset -= sizeof(u32);
        }
        return patch;
    }
}
