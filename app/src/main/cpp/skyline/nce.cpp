#include <sched.h>
#include <linux/uio.h>
#include <linux/elf.h>
#include <os.h>

extern bool Halt;

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
        while (!Halt && !state.os->processMap.empty()) {
            for (const auto &process : state.os->processMap) { // NOLINT(performance-for-range-copy)
                state.os->thisProcess = process.second;
                state.os->thisThread = process.second->threadMap.at(process.first);
                currPid = process.first;
                auto &currRegs = registerMap[currPid];
                if (state.thisThread->status == kernel::type::KThread::Status::Running) {
                    if (waitpid(state.thisThread->pid, &status, WNOHANG) == state.thisThread->pid) {
                        if (WIFSTOPPED(status) && (WSTOPSIG(status) == SIGTRAP || WSTOPSIG(status) == SIGSTOP)) { // NOLINT(hicpp-signed-bitwise)
                            ReadRegisters(currRegs);
                            auto instr = ReadBrk(currRegs.pc);
                            if (instr.Verify()) {
                                // We store the instruction value as the immediate value in BRK. 0x0 to 0x7F are SVC, 0x80 to 0x9E is MRS for TPIDRRO_EL0.
                                if (instr.value <= constant::SvcLast) {
                                    state.os->SvcHandler(static_cast<u16>(instr.value));
                                    if (state.thisThread->status != kernel::type::KThread::Status::Running)
                                        continue;
                                } else if (instr.value > constant::SvcLast && instr.value <= constant::SvcLast + constant::NumRegs) {
                                    // Catch MRS that reads the value of TPIDRRO_EL0 (TLS)
                                    SetRegister(static_cast<Xreg>(instr.value - (constant::SvcLast + 1)), state.thisThread->tls);
                                } else if (instr.value == constant::BrkRdy)
                                    continue;
                                else
                                    throw exception("Received unhandled BRK: 0x{:X}", static_cast<u64>(instr.value));
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
                } else if ((state.thisThread->status == kernel::type::KThread::Status::WaitSync || state.thisThread->status == kernel::type::KThread::Status::Sleeping || state.thisThread->status == kernel::type::KThread::Status::WaitCondVar) && state.thisThread->timeout != 0) { // timeout == 0 means sleep forever
                    if (state.thisThread->timeout <= GetCurrTimeNs()) {
                        state.logger->Info("An event has timed out: {}", state.thisThread->status);
                        if (state.thisThread->status == kernel::type::KThread::Status::WaitSync || state.thisThread->status == kernel::type::KThread::Status::WaitCondVar)
                            SetRegister(Wreg::W0, constant::status::Timeout);
                        state.thisThread->status = kernel::type::KThread::Status::Runnable;
                    }
                }
                if (state.thisThread->status == kernel::type::KThread::Status::Runnable) {
                    state.thisThread->ClearWaitObjects();
                    state.thisThread->status = kernel::type::KThread::Status::Running;
                    currRegs.pc += sizeof(u32);
                    WriteRegisters(currRegs);
                    ResumeProcess();
                }
            }
            state.os->serviceManager.Loop();
            state.gpu->Loop();
        }
        for (const auto &process : state.os->processMap) {
            state.os->KillThread(process.first);
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
        state.logger->Debug("CPU Context:");
        state.logger->Debug("SP: 0x{:X}", regs.sp);
        state.logger->Debug("PSTATE: 0x{:X}", regs.pstate);
        for (u16 index = 0; index < constant::NumRegs - 2; index++) {
            state.logger->Debug("X{}: 0x{:X}", index, regs.regs[index]);
        }
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
}
