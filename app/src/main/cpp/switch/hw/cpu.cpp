#include "cpu.h"
extern bool halt;

namespace lightSwitch::hw {
    Cpu::~Cpu() {
        if (child) kill(child, SIGKILL);
    }

    long *Cpu::ReadMemory(uint64_t address) { // Return a single word (32-bit)
        status = ptrace(PTRACE_PEEKDATA, child, address, NULL);
        if (status == -1) throw std::runtime_error("Cannot read memory");
        return &status;
    }

    void Cpu::WriteMemory(uint64_t address) { // Write a single word (32-bit)
        status = ptrace(PTRACE_GETREGSET, child, NT_PRSTATUS, &iov);
        if (status == -1) throw std::runtime_error("Cannot write memory");
    }

    void Cpu::ReadRegisters() { // Read all registers into 'regs'
        iov = {&regs, sizeof(regs)};
        status = ptrace(PTRACE_GETREGSET, child, NT_PRSTATUS, &iov);
        if (status == -1) throw std::runtime_error("Cannot read registers");
    }

    void Cpu::WriteRegisters() { // Write all registers from 'regs'
        iov = {&regs, sizeof(regs)};
        status = ptrace(PTRACE_SETREGSET, child, NT_PRSTATUS, &iov);
        if (status == -1) throw std::runtime_error("Cannot write registers");
    }

    void Cpu::ResumeProcess() { // Resumes a process stopped due to a signal
        status = ptrace(PTRACE_CONT, child, NULL, NULL);
        if (status == -1) throw std::runtime_error("Cannot resume process");
    }

    void Cpu::WriteBreakpoint(uint64_t address_, uint64_t size) {
        auto address = (uint32_t *) address_;
        for (uint64_t iter = 0; iter < size; iter++) {
            auto instr_svc = reinterpret_cast<instr::svc *>(address + iter);
            auto instr_mrs = reinterpret_cast<instr::mrs *>(address + iter);

            if (instr_svc->verify()) {
                if(debug_build)
                    syslog(LOG_WARNING, "Found SVC call: 0x%X, At location 0x%X", instr_svc->value, address+iter);
                instr::brk brk(static_cast<uint16_t>(instr_svc->value));
                address[iter] = *reinterpret_cast<uint32_t *>(&brk);
            } else if (instr_mrs->verify() && instr_mrs->src_reg == constant::tpidrro_el0) {
                if(debug_build)
                    syslog(LOG_WARNING, "Found MRS call: 0x%X, At location 0x%X", instr_mrs->dst_reg, address+iter);
                instr::brk brk(static_cast<uint16_t>(constant::svc_last + 1 + instr_mrs->dst_reg));
                address[iter] = *reinterpret_cast<uint32_t *>(&brk);
            }
        }
    }

    void Cpu::Execute(Memory::Region region, std::shared_ptr<Memory> memory, std::function<void(uint16_t, void *)> svc_handler, void *device) {
        hw::Memory::RegionData exec = memory->region_map.at(region);
        WriteBreakpoint(exec.address, exec.size); // We write the BRK instructions to replace SVC & MRS so we receive a breakpoint
        child = ExecuteChild(exec.address);
        while (waitpid(child, &pid_status, 0)) {
            if (WIFSTOPPED(pid_status)) {
                ReadRegisters();
                if(debug_build)
                    syslog(LOG_INFO, "PC is at 0x%X", regs.pc);
                if (!regs.pc || regs.pc == 0xBADC0DE) break;
                // We store the instruction value as the immediate value. 0x0 to 0x7F are SVC, 0x80 to 0x9E is MRS for TPIDRRO_EL0.
                auto instr = reinterpret_cast<instr::brk *>(ReadMemory(regs.pc));
                if (instr->verify()) {
                    if (instr->value <= constant::svc_last) {
                        svc_handler(static_cast<uint16_t>(instr->value), device);
                        if (debug_build)
                            syslog(LOG_ERR, "SVC has been called 0x%X", instr->value);
                        if (halt) break;
                    } else if (instr->value > constant::svc_last && instr->value <= constant::svc_last + constant::num_regs) {
                        // Catch MRS that reads the value of TPIDRRO_EL0 (TLS)
                        // https://switchbrew.org/wiki/Thread_Local_Storage
                        SetRegister(xreg(instr->value - (constant::svc_last + 1)), tls);
                        if (debug_build)
                            syslog(LOG_ERR, "MRS has been called 0x%X", instr->value - (constant::svc_last + 1));
                    } else syslog(LOG_ERR, "Received unhandled BRK 0x%X", instr->value);
                }
                regs.pc += 4; // Increment program counter by a single instruction (32 bits)
                WriteRegisters();
            } else if (WIFEXITED(pid_status)) break;
            ResumeProcess();
        }
        kill(child, SIGABRT);
        child = 0;
        pid_status = 0;
        halt = false;
    }

    pid_t Cpu::ExecuteChild(uint64_t address) {
        pid_t pid = fork();
        if (!pid) {
            ptrace(PTRACE_TRACEME, 0, NULL, NULL);
            asm volatile("br %0" :: "r"(address));
        }
        return pid;
    }

    void Cpu::StopExecution() { halt = true; }

    uint64_t Cpu::GetRegister(xreg reg_id) {
        return regs.regs[reg_id];
    }

    void Cpu::SetRegister(xreg reg_id, uint64_t value) {
        regs.regs[reg_id] = value;
    }

    uint64_t Cpu::GetRegister(wreg reg_id) {
        return (reinterpret_cast<uint32_t *>(&regs.regs))[wreg_lut[reg_id]];
    }

    void Cpu::SetRegister(wreg reg_id, uint32_t value) {
        (reinterpret_cast<uint32_t *>(&regs.regs))[wreg_lut[reg_id]] = value;
    }
}