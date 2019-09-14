#include <sched.h>
#include <linux/uio.h>
#include <linux/elf.h>
#include "os.h"
#include "nce.h"

extern bool halt;

namespace lightSwitch {
    void NCE::ReadRegisters(user_pt_regs &registers, pid_t pid) const {
        iovec iov = {&registers, sizeof(registers)};
        long status = ptrace(PTRACE_GETREGSET, pid ? pid : curr_pid, NT_PRSTATUS, &iov);
        if (status == -1) throw exception(fmt::format("Cannot read registers, PID: {}, Error: {}", pid, strerror(errno)));
    }

    void NCE::WriteRegisters(user_pt_regs &registers, pid_t pid) const {
        iovec iov = {&registers, sizeof(registers)};
        long status = ptrace(PTRACE_SETREGSET, pid ? pid : curr_pid, NT_PRSTATUS, &iov);
        if (status == -1) throw exception(fmt::format("Cannot write registers, PID: {}, Error: {}", pid, strerror(errno)));
    }

    instr::brk NCE::ReadBrk(u64 address, pid_t pid) const {
        long status = ptrace(PTRACE_PEEKDATA, pid ? pid : curr_pid, address, NULL);
        if (status == -1) throw exception(fmt::format("Cannot read instruction from memory, Address: {}, PID: {}, Error: {}", address, pid, strerror(errno)));
        return *(reinterpret_cast<instr::brk *>(&status));
    }

    void NCE::Initialize(const device_state &state) {
        this->state = &state;
    }

    void NCE::Execute() {
        int status = 0;
        while (!halt && !state->os->process_map.empty() && ((curr_pid = wait(&status)) != -1)) {
            if (WIFSTOPPED(status) && (WSTOPSIG(status) == SIGTRAP || WSTOPSIG(status) == SIGSTOP)) { // NOLINT(hicpp-signed-bitwise)
                auto &curr_regs = register_map[curr_pid];
                ReadRegisters(curr_regs);
                auto instr = ReadBrk(curr_regs.pc);
                if (instr.verify()) {
                    // We store the instruction value as the immediate value in BRK. 0x0 to 0x7F are SVC, 0x80 to 0x9E is MRS for TPIDRRO_EL0.
                    if (instr.value <= constant::svc_last) {
                        state->os->SvcHandler(static_cast<u16>(instr.value), curr_pid);
                    } else if (instr.value > constant::svc_last && instr.value <= constant::svc_last + constant::num_regs) {
                        // Catch MRS that reads the value of TPIDRRO_EL0 (TLS)
                        SetRegister(static_cast<xreg>(instr.value - (constant::svc_last + 1)), state->os->process_map.at(curr_pid)->thread_map.at(curr_pid)->tls);
                        state->logger->Write(Logger::DEBUG, "\"MRS X{}, TPIDRRO_EL0\" has been called", instr.value - (constant::svc_last + 1));
                    } else if (instr.value == constant::brk_rdy)
                        continue;
                    else
                        throw exception(fmt::format("Received unhandled BRK: 0x{:X}", static_cast<u64>(instr.value)));
                }
                curr_regs.pc += 4; // Increment program counter by a single instruction (32 bits)
                WriteRegisters(curr_regs);
            } else {
                state->logger->Write(Logger::DEBUG, "Thread threw unknown signal, PID: {}, Stop Signal: {}", curr_pid, strsignal(WSTOPSIG(status))); // NOLINT(hicpp-signed-bitwise)
                state->os->KillThread(curr_pid);
            }
            ResumeProcess();
        }
    }

    void brk_lr() {
        asm("BRK #0xFF"); // BRK #constant::brk_rdy
    }

    void NCE::ExecuteFunction(void *func, user_pt_regs &func_regs, pid_t pid) {
        pid = pid ? pid : curr_pid;
        bool was_running = PauseProcess(pid);
        user_pt_regs backup_regs{};
        ReadRegisters(backup_regs, pid);
        func_regs.pc = reinterpret_cast<u64>(func);
        func_regs.sp = backup_regs.sp;
        func_regs.regs[static_cast<uint>(xreg::x30)] = reinterpret_cast<u64>(brk_lr); // Set LR to 'brk_lr' so the application will hit a breakpoint after the function returns [LR is where the program goes after it returns from a function]
        WriteRegisters(func_regs, pid);
        ResumeProcess(pid);
        func_regs = WaitRdy(pid);
        WriteRegisters(backup_regs, pid);
        if (was_running)
            ResumeProcess(pid);
    }

    user_pt_regs NCE::WaitRdy(pid_t pid) {
        int status;
        user_pt_regs regs{};
        waitpid(pid, &status, 0);
        if (WIFSTOPPED(status) && (WSTOPSIG(status) == SIGTRAP)) { // NOLINT(hicpp-signed-bitwise)
            ReadRegisters(regs, pid);
            auto instr = ReadBrk(regs.pc, pid);
            if (instr.verify() && instr.value == constant::brk_rdy) {
                regs.pc += 4; // Increment program counter by a single instruction (32 bits)
                WriteRegisters(regs, pid);
                return regs;
            } else
                throw exception(fmt::format("An unknown BRK was hit during WaitRdy, PID: {}, BRK value: {}", pid, static_cast<u64>(instr.value)));
        } else
            throw exception(fmt::format("An unknown signal was caused during WaitRdy, PID: {}, Status: 0x{:X}, Signal: {}", pid, status, strsignal(WSTOPSIG(status)))); // NOLINT(hicpp-signed-bitwise)
    }

    bool NCE::PauseProcess(pid_t pid) const {
        pid = pid ? pid : curr_pid;
        int status = 0;
        waitpid(pid, &status, WNOHANG);
        bool was_stopped = WIFSTOPPED(status); // NOLINT(hicpp-signed-bitwise)
        if (was_stopped) {
            if ((kill(pid, SIGSTOP) != -1) && (waitpid(pid, nullptr, 0) != -1)) return true;
            else throw exception(fmt::format("Cannot pause process: {}, Error: {}", pid, strerror(errno)));
        } else return false;
    }

    void NCE::ResumeProcess(pid_t pid) const {
        long status = ptrace(PTRACE_CONT, pid ? pid : curr_pid, NULL, NULL);
        if (status == -1) throw exception(fmt::format("Cannot resume process: {}, Error: {}", pid, strerror(errno)));
    }

    void NCE::StartProcess(u64 entry_point, u64 entry_arg, u64 stack_top, u32 handle, pid_t pid) const {
        user_pt_regs regs{0};
        regs.sp = stack_top;
        regs.pc = entry_point;
        regs.regs[0] = entry_arg;
        regs.regs[1] = handle;
        WriteRegisters(regs, pid);
        ResumeProcess(pid);
    }

    u64 NCE::GetRegister(xreg reg_id, pid_t pid) {
        return register_map.at(pid ? pid : curr_pid).regs[static_cast<uint>(reg_id)];
    }

    void NCE::SetRegister(xreg reg_id, u64 value, pid_t pid) {
        register_map.at(pid ? pid : curr_pid).regs[static_cast<uint>(reg_id)] = value;
    }

    u64 NCE::GetRegister(wreg reg_id, pid_t pid) {
        return (reinterpret_cast<u32 *>(&register_map.at(pid ? pid : curr_pid).regs))[static_cast<uint>(reg_id) * 2];
    }

    void NCE::SetRegister(wreg reg_id, u32 value, pid_t pid) {
        (reinterpret_cast<u32 *>(&register_map.at(pid ? pid : curr_pid).regs))[static_cast<uint>(reg_id) * 2] = value;
    }

    u64 NCE::GetRegister(sreg reg_id, pid_t pid) {
        pid = pid ? pid : curr_pid;
        switch (reg_id) {
            case sreg::pc:
                return register_map.at(pid).pc;
            case sreg::sp:
                return register_map.at(pid).sp;
            case sreg::pstate:
                return register_map.at(pid).pstate;
        }
    }

    void NCE::SetRegister(sreg reg_id, u32 value, pid_t pid) {
        pid = pid ? pid : curr_pid;
        switch (reg_id) {
            case sreg::pc:
                register_map.at(pid).pc = value;
            case sreg::sp:
                register_map.at(pid).sp = value;
            case sreg::pstate:
                register_map.at(pid).pstate = value;
        }
    }

    std::shared_ptr<kernel::type::KSharedMemory> NCE::MapSharedRegion(const u64 address, const size_t size, const Memory::Permission local_permission, const Memory::Permission remote_permission, const Memory::Type type, const Memory::Region region) {
        auto item = std::make_shared<kernel::type::KSharedMemory>(*state, size, local_permission, remote_permission, type, 0, 0);
        item->Map(address);
        memory_map[item->address] = item;
        memory_region_map[region] = item;
        return item;
    }

    size_t NCE::GetSharedSize() {
        size_t shared_size = 0;
        for (auto &region : memory_map) {
            shared_size += region.second->size;
        }
        return shared_size;
    }
}
