#include <sched.h>
#include <linux/uio.h>
#include <linux/elf.h>
#include "os.h"
#include "jvm.h"
#include "guest.h"

extern bool Halt;
extern skyline::Mutex jniMtx;

namespace skyline {
    void NCE::ReadRegisters(user_pt_regs &registers, pid_t pid) const {
        iovec iov = {&registers, sizeof(registers)};
        long status = ptrace(PTRACE_GETREGSET, pid ? pid : currPid, NT_PRSTATUS, &iov);
        if (status == -1)
            throw exception("Cannot read registers, PID: {}, Error: {}", pid, strerror(errno));
    }

    void NCE::WriteRegisters(user_pt_regs &registers, pid_t pid) const {
        iovec iov = {&registers, sizeof(registers)};
        long status = ptrace(PTRACE_SETREGSET, pid ? pid : currPid, NT_PRSTATUS, &iov);
        if (status == -1)
            throw exception("Cannot write registers, PID: {}, Error: {}", pid, strerror(errno));
    }

    instr::Brk NCE::ReadBrk(u64 address, pid_t pid) const {
        long status = ptrace(PTRACE_PEEKDATA, pid ? pid : currPid, address, NULL);
        if (status == -1)
            throw exception("Cannot read instruction from memory, Address: {}, PID: {}, Error: {}", address, pid, strerror(errno));
        return *(reinterpret_cast<instr::Brk *>(&status));
    }

    NCE::NCE(const DeviceState &state) : state(state) {}

    void NCE::Execute() {
        int status = 0;
        while (!Halt && state.os->process) {
            std::lock_guard jniGd(jniMtx);
            for (const auto &thread : state.os->process->threadMap) {
                state.os->thisThread = thread.second;
                currPid = thread.first;
                auto &currRegs = registerMap[currPid];
                if (state.thread->status == kernel::type::KThread::Status::Running) {
                    if (waitpid(state.thread->pid, &status, WNOHANG) == state.thread->pid) {
                        if (WIFSTOPPED(status) && (WSTOPSIG(status) == SIGTRAP || WSTOPSIG(status) == SIGSTOP)) { // NOLINT(hicpp-signed-bitwise)
                            ReadRegisters(currRegs);
                            auto instr = ReadBrk(currRegs.pc);
                            if (instr.Verify()) {
                                // We store the instruction value as the immediate value in BRK. 0x0 to 0x7F are SVC, 0x80 to 0x9E is MRS for TPIDRRO_EL0.
                                if (instr.value <= constant::SvcLast) {
                                    state.os->SvcHandler(static_cast<u16>(instr.value));
                                    if (state.thread->status != kernel::type::KThread::Status::Running)
                                        continue;
                                } else if (instr.value > constant::SvcLast && instr.value <= constant::SvcLast + constant::NumRegs) {
                                    // Catch MRS that reads the value of TPIDRRO_EL0 (TLS)
                                    SetRegister(static_cast<Xreg>(instr.value - (constant::SvcLast + 1)), state.thread->tls);
                                } else if (instr.value == constant::BrkRdy)
                                    continue;
                                else {
                                    ProcessTrace();
                                    throw exception("Received unhandled BRK: 0x{:X}", static_cast<u64>(instr.value));
                                }
                            }
                            currRegs.pc += sizeof(u32);
                            WriteRegisters(currRegs);
                            ResumeProcess();
                        } else {
                            try {
                                state.logger->Warn("Thread threw unknown signal, PID: {}, Stop Signal: {}", currPid, strsignal(WSTOPSIG(status))); // NOLINT(hicpp-signed-bitwise)
                                ProcessTrace();
                            } catch (const exception &) {
                                state.logger->Warn("Thread threw unknown signal, PID: {}, Stop Signal: {}", currPid, strsignal(WSTOPSIG(status))); // NOLINT(hicpp-signed-bitwise)
                            }
                            state.os->KillThread(currPid);
                        }
                    }
                } else if ((state.thread->status == kernel::type::KThread::Status::WaitSync || state.thread->status == kernel::type::KThread::Status::Sleeping || state.thread->status == kernel::type::KThread::Status::WaitCondVar) && state.thread->timeout != 0) { // timeout == 0 means sleep forever
                    if (state.thread->timeout <= GetCurrTimeNs()) {
                        if(state.thread->status != kernel::type::KThread::Status::Sleeping)
                            state.logger->Info("An event has timed out: {}", state.thread->status);
                        if (state.thread->status == kernel::type::KThread::Status::WaitSync || state.thread->status == kernel::type::KThread::Status::WaitCondVar)
                            SetRegister(Wreg::W0, constant::status::Timeout);
                        state.thread->status = kernel::type::KThread::Status::Runnable;
                    }
                }
                if (state.thread->status == kernel::type::KThread::Status::Runnable) {
                    state.thread->ClearWaitObjects();
                    state.thread->status = kernel::type::KThread::Status::Running;
                    currRegs.pc += sizeof(u32);
                    WriteRegisters(currRegs);
                    ResumeProcess();
                }
            }
            state.os->serviceManager.Loop();
            state.gpu->Loop();
        }
    }

    void BrkLr() {
        asm("BRK #0xFF"); // BRK #constant::brkRdy
    }

    void NCE::ExecuteFunction(void *func, user_pt_regs &funcRegs, pid_t pid) {
        pid = pid ? pid : currPid;
        bool wasRunning = PauseProcess(pid);
        user_pt_regs backupRegs{};
        ReadRegisters(backupRegs, pid);
        funcRegs.pc = reinterpret_cast<u64>(func);
        funcRegs.sp = backupRegs.sp;
        funcRegs.regs[static_cast<uint>(Xreg::X30)] = reinterpret_cast<u64>(BrkLr); // Set LR to 'brk_lr' so the application will hit a breakpoint after the function returns
        WriteRegisters(funcRegs, pid);
        ResumeProcess(pid);
        funcRegs = WaitRdy(pid);
        WriteRegisters(backupRegs, pid);
        if (wasRunning)
            ResumeProcess(pid);
    }

    user_pt_regs NCE::WaitRdy(pid_t pid) {
        int status;
        user_pt_regs regs{};
        waitpid(pid, &status, 0);
        if (WIFSTOPPED(status) && (WSTOPSIG(status) == SIGTRAP)) { // NOLINT(hicpp-signed-bitwise)
            ReadRegisters(regs, pid);
            auto instr = ReadBrk(regs.pc, pid);
            if (instr.Verify() && instr.value == constant::BrkRdy) {
                regs.pc += 4; // Increment program counter by a single instruction (32 bits)
                WriteRegisters(regs, pid);
                return regs;
            } else
                throw exception("An unknown BRK was hit during WaitRdy, PID: {}, BRK value: {}", pid, static_cast<u64>(instr.value));
        } else
            throw exception("An unknown signal was caused during WaitRdy, PID: {}, Status: 0x{:X}, Signal: {}", pid, status, strsignal(WSTOPSIG(status))); // NOLINT(hicpp-signed-bitwise)
    }

    bool NCE::PauseProcess(pid_t pid) const {
        pid = pid ? pid : currPid;
        int status = 0;
        waitpid(pid, &status, WNOHANG);
        bool wasStopped = WIFSTOPPED(status); // NOLINT(hicpp-signed-bitwise)
        if (wasStopped) {
            if ((kill(pid, SIGSTOP) != -1) && (waitpid(pid, nullptr, WNOHANG) != -1))
                return true;
            else
                throw exception("Cannot pause process: {}, Error: {}", pid, strerror(errno));
        } else
            return false;
    }

    void NCE::ResumeProcess(pid_t pid) const {
        long status = ptrace(PTRACE_CONT, pid ? pid : currPid, NULL, NULL);
        if (status == -1)
            throw exception("Cannot resume process: {}, Error: {}", pid, strerror(errno));
    }

    void NCE::StartProcess(u64 entryPoint, u64 entryArg, u64 stackTop, u32 handle, pid_t pid) const {
        user_pt_regs regs{0};
        regs.sp = stackTop;
        regs.pc = entryPoint;
        regs.regs[0] = entryArg;
        regs.regs[1] = handle;
        WriteRegisters(regs, pid);
        ResumeProcess(pid);
    }

    void NCE::ProcessTrace(u16 numHist, pid_t pid) {
        pid = pid ? pid : currPid;
        user_pt_regs regs{};
        ReadRegisters(regs, pid);
        u64 offset = regs.pc - (sizeof(u32) * numHist);
        std::string raw{};
        state.logger->Debug("Process Trace:");
        for (; offset <= (regs.pc + sizeof(u32)); offset += sizeof(u32)) {
            u32 instr = __builtin_bswap32(static_cast<u32>(ptrace(PTRACE_PEEKDATA, pid, offset, NULL)));
            if (offset == regs.pc)
                state.logger->Debug("-> 0x{:X} : 0x{:08X}", offset, instr);
            else
                state.logger->Debug("   0x{:X} : 0x{:08X}", offset, instr);
            raw += fmt::format("{:08X}", instr);
        }
        state.logger->Debug("Raw Instructions: 0x{}", raw);
        std::string regStr;
        for (u16 index = 0; index < constant::NumRegs - 1; index+=2) {
            regStr += fmt::format("\nX{}: 0x{:X}, X{}: 0x{:X}", index, regs.regs[index], index+1, regs.regs[index+1]);
        }
        state.logger->Debug("CPU Context:\nSP: 0x{:X}\nLR: 0x{:X}\nPSTATE: 0x{:X}{}", regs.sp, regs.regs[30], regs.pstate, regStr);
    }

    u64 NCE::GetRegister(Xreg regId, pid_t pid) {
        return registerMap.at(pid ? pid : currPid).regs[static_cast<uint>(regId)];
    }

    void NCE::SetRegister(Xreg regId, u64 value, pid_t pid) {
        registerMap.at(pid ? pid : currPid).regs[static_cast<uint>(regId)] = value;
    }

    u32 NCE::GetRegister(Wreg regId, pid_t pid) {
        return (reinterpret_cast<u32 *>(&registerMap.at(pid ? pid : currPid).regs))[static_cast<uint>(regId) * 2];
    }

    void NCE::SetRegister(Wreg regId, u32 value, pid_t pid) {
        (reinterpret_cast<u32 *>(&registerMap.at(pid ? pid : currPid).regs))[static_cast<uint>(regId) * 2] = value;
    }

    u64 NCE::GetRegister(Sreg regId, pid_t pid) {
        pid = pid ? pid : currPid;
        switch (regId) {
            case Sreg::Pc:
                return registerMap.at(pid).pc;
            case Sreg::Sp:
                return registerMap.at(pid).sp;
            case Sreg::PState:
                return registerMap.at(pid).pstate;
        }
    }

    void NCE::SetRegister(Sreg regId, u32 value, pid_t pid) {
        pid = pid ? pid : currPid;
        switch (regId) {
            case Sreg::Pc:
                registerMap.at(pid).pc = value;
            case Sreg::Sp:
                registerMap.at(pid).sp = value;
            case Sreg::PState:
                registerMap.at(pid).pstate = value;
        }
    }

    std::vector<u32> NCE::PatchCode(std::vector<u8>& code, i64 offset) {
        u32 *address = reinterpret_cast<u32 *>(code.data());
        u32 *end = address + (code.size() / sizeof(u32));
        i64 patchOffset = offset;

        std::vector<u32> patch((guest::saveCtxSize + guest::loadCtxSize) / sizeof(u32));
        std::memcpy(patch.data(), reinterpret_cast<void*>(&guest::saveCtx), guest::saveCtxSize);
        offset += guest::saveCtxSize;

        std::memcpy(reinterpret_cast<u8*>(patch.data()) + guest::saveCtxSize,
                    reinterpret_cast<void*>(&guest::loadCtx), guest::loadCtxSize);
        offset += guest::loadCtxSize;

        while (address < end) {
            auto instrSvc = reinterpret_cast<instr::Svc *>(address);
            auto instrMrs = reinterpret_cast<instr::Mrs *>(address);

            if (instrSvc->Verify()) {
                instr::B bjunc(offset);
                constexpr u32 strLr = 0xF81F0FFE; // STR LR, [SP, #-16]!
                offset += sizeof(strLr);
                instr::Brk brk(static_cast<u16>(instrSvc->value));
                offset += sizeof(brk);
                instr::BL bSvCtx(patchOffset - offset);
                offset += sizeof(bSvCtx);
                instr::BL bLdCtx((patchOffset + guest::saveCtxSize) - offset);
                offset += sizeof(bLdCtx);
                constexpr u32 ldrLr = 0xF84107FE; // LDR LR, [SP], #16
                offset += sizeof(ldrLr);
                instr::B bret(-offset + sizeof(u32));
                offset += sizeof(bret);

                *address = bjunc.raw;
                patch.push_back(strLr);
                patch.push_back(brk.raw);
                patch.push_back(bSvCtx.raw);
                patch.push_back(bLdCtx.raw);
                patch.push_back(ldrLr);
                patch.push_back(bret.raw);
            }  else if (instrMrs->Verify()) {
                if (instrMrs->srcReg == constant::TpidrroEl0) {
                    instr::B bjunc(offset);
                    instr::Brk brk(static_cast<u16>(constant::SvcLast + 1 + instrMrs->dstReg));
                    offset += sizeof(u32);
                    instr::B bret(-offset + sizeof(u32));
                    offset += sizeof(u32);

                    *address = bjunc.raw;
                    patch.push_back(brk.raw);
                    patch.push_back(bret.raw);
                } else if (instrMrs->srcReg == constant::CntpctEl0) {
                    instr::Mrs mrs(constant::CntvctEl0, instrMrs->dstReg);
                    *address = mrs.raw;
                }
            }
            address++;
            offset -= sizeof(u32);
            patchOffset -= sizeof(u32);
        }
        return patch;
    }
}

