#include <sched.h>
#include <linux/uio.h>
#include <linux/elf.h>
#include "os.h"
#include "jvm.h"
#include "guest.h"

extern bool Halt;
extern skyline::Mutex jniMtx;

namespace skyline {
    namespace instr {
        /**
         * @brief A bit-field struct that encapsulates a BRK instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/brk-breakpoint-instruction.
         */
        struct Brk {
            /**
             * @brief Creates a BRK instruction with a specific immediate value, used for generating BRK opcodes
             * @param value The immediate value of the instruction
             */
            explicit Brk(u16 value) {
                start = 0x0; // First 5 bits of a BRK instruction are 0
                this->value = value;
                end = 0x6A1; // Last 11 bits of a BRK instruction stored as u16
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid BRK instruction
             */
            inline bool Verify() {
                return (start == 0x0 && end == 0x6A1);
            }

            union {
                struct {
                    u8 start  : 5;
                    u32 value : 16;
                    u16 end   : 11;
                };
                u32 raw{};
            };
        };

        static_assert(sizeof(Brk) == sizeof(u32));

        /**
         * @brief A bit-field struct that encapsulates a SVC instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/svc-supervisor-call.
         */
        struct Svc {
            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid SVC instruction
             */
            inline bool Verify() {
                return (start == 0x1 && end == 0x6A0);
            }

            union {
                struct {
                    u8 start  : 5;
                    u32 value : 16;
                    u16 end   : 11;
                };
                u32 raw{};
            };
        };

        static_assert(sizeof(Svc) == sizeof(u32));

        /**
         * @brief A bit-field struct that encapsulates a MRS instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/mrs-move-system-register.
         */
        struct Mrs {
            /**
             * @brief Creates a MRS instruction, used for generating BRK opcodes
             * @param srcReg The source system register
             * @param dstReg The destination Xn register
             */
            Mrs(u32 srcReg, u8 dstReg) {
                this->srcReg = srcReg;
                this->dstReg = dstReg;
                end = 0xD53; // Last 12 bits of a MRS instruction stored as u16
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid MRS instruction
             */
            inline bool Verify() {
                return (end == 0xD53);
            }

            union {
                struct {
                    u8 dstReg  : 5;
                    u32 srcReg : 15;
                    u16 end    : 12;
                };
                u32 raw{};
            };
        };

        static_assert(sizeof(Mrs) == sizeof(u32));

        /**
         * @brief A bit-field struct that encapsulates a B instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/b-branch.
         */
        struct B {
        public:
            explicit B(i64 offset) {
                this->offset = static_cast<i32>(offset / 4);
                end = 0x5;
            }

            /**
             * @brief Returns the offset of the instruction
             * @return The offset encoded within the instruction
             */
            inline i32 Offset() {
                return offset * 4;
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid Branch instruction
             */
            inline bool Verify() {
                return (end == 0x5);
            }

            union {
                struct {
                    i32 offset : 26;
                    u8 end     : 6;
                };
                u32 raw{};
            };
        };

        static_assert(sizeof(B) == sizeof(u32));

        /**
         * @brief A bit-field struct that encapsulates a BL instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/b-branch.
         */
        struct BL {
        public:
            explicit BL(i64 offset) {
                this->offset = static_cast<i32>(offset / 4);
                end = 0x25;
            }

            /**
             * @brief Returns the offset of the instruction
             * @return The offset encoded within the instruction
             */
            inline i32 Offset() {
                return offset * 4;
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid Branch instruction
             */
            inline bool Verify() {
                return (end == 0x85);
            }

            union {
                struct {
                    i32 offset : 26;
                    u8 end     : 6;
                };
                u32 raw{};
            };
        };

        static_assert(sizeof(BL) == sizeof(u32));
    }

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
            std::lock_guard jniGd(jniMtx);
            for (const auto &process : state.os->processMap) {
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

