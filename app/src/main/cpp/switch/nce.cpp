#include <sched.h>
#include <linux/uio.h>
#include <linux/elf.h>
#include <fcntl.h>
#include <unistd.h>
#include <android/sharedmem.h>
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

    instr::brk NCE::ReadBrk(uint64_t address, pid_t pid) const {
        long status = ptrace(PTRACE_PEEKDATA, pid ? pid : curr_pid, address, NULL);
        if (status == -1) throw exception(fmt::format("Cannot read instruction from memory, Address: {}, PID: {}, Error: {}", address, pid, strerror(errno)));
        return *(reinterpret_cast<instr::brk *>(&status));
    }

    NCE::~NCE() {
        for (auto&region : region_memory_map) {
            munmap(reinterpret_cast<void *>(region.second.address), region.second.size);
        };
    }

    void NCE::Execute(const device_state &state) {
        this->state = const_cast<device_state *>(&state);
        int status;
        while (!halt && !state.os->process_map.empty() && ((curr_pid = wait(&status)) != -1)) {
            if (WIFSTOPPED(status) && (WSTOPSIG(status) == SIGTRAP || WSTOPSIG(status) == SIGSTOP)) { // NOLINT(hicpp-signed-bitwise)
                auto& curr_regs = register_map[curr_pid];
                ReadRegisters(curr_regs);
                auto instr = ReadBrk(curr_regs.pc);
                if (instr.verify()) {
                    // We store the instruction value as the immediate value in BRK. 0x0 to 0x7F are SVC, 0x80 to 0x9E is MRS for TPIDRRO_EL0.
                    if (instr.value <= constant::svc_last) {
                        state.os->SvcHandler(static_cast<uint16_t>(instr.value), curr_pid);
                    } else if (instr.value > constant::svc_last && instr.value <= constant::svc_last + constant::num_regs) {
                        // Catch MRS that reads the value of TPIDRRO_EL0 (TLS)
                        SetRegister(regs::xreg(instr.value - (constant::svc_last + 1)), state.os->process_map.at(curr_pid)->thread_map.at(curr_pid)->tls);
                        state.logger->Write(Logger::DEBUG, "\"MRS X{}, TPIDRRO_EL0\" has been called", instr.value - (constant::svc_last + 1));
                    } else if (instr.value == constant::brk_rdy)
                        continue;
                    else
                        throw exception(fmt::format("Received unhandled BRK: 0x{:X}", static_cast<uint64_t>(instr.value)));
                }
                curr_regs.pc += 4; // Increment program counter by a single instruction (32 bits)
                WriteRegisters(curr_regs);
            } else {
                auto& curr_regs = register_map[curr_pid];
                ReadRegisters(curr_regs);
                state.logger->Write(Logger::DEBUG, "Thread threw unknown signal, PID: {}, Status: 0x{:X}, INSTR: 0x{:X}", curr_pid, status, *(uint32_t*)curr_regs.pc);
                state.os->KillThread(curr_pid);
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
        func_regs.pc = reinterpret_cast<uint64_t>(func);
        func_regs.sp = backup_regs.sp;
        func_regs.regs[regs::x30] = reinterpret_cast<uint64_t>(brk_lr); // Set LR to 'brk_lr' so the application will hit a breakpoint after the function returns [LR is where the program goes after it returns from a function]
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
                throw exception(fmt::format("An unknown BRK was hit during WaitRdy, PID: {}, BRK value: {}", pid, static_cast<uint64_t>(instr.value)));
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

    void NCE::StartProcess(uint64_t entry_point, uint64_t entry_arg, uint64_t stack_top, uint32_t handle, pid_t pid) const {
        user_pt_regs regs{0};
        regs.sp = stack_top;
        regs.pc = entry_point;
        regs.regs[0] = entry_arg;
        regs.regs[1] = handle;
        WriteRegisters(regs, pid);
        ResumeProcess(pid);
    }

    uint64_t NCE::GetRegister(regs::xreg reg_id, pid_t pid) {
        return register_map.at(pid ? pid : curr_pid).regs[reg_id];
    }

    void NCE::SetRegister(regs::xreg reg_id, uint64_t value, pid_t pid) {
        register_map.at(pid ? pid : curr_pid).regs[reg_id] = value;
    }

    uint64_t NCE::GetRegister(regs::wreg reg_id, pid_t pid) {
        return (reinterpret_cast<uint32_t *>(&register_map.at(pid ? pid : curr_pid).regs))[reg_id * 2];
    }

    void NCE::SetRegister(regs::wreg reg_id, uint32_t value, pid_t pid) {
        (reinterpret_cast<uint32_t *>(&register_map.at(pid ? pid : curr_pid).regs))[reg_id * 2] = value;
    }

    uint64_t NCE::GetRegister(regs::sreg reg_id, pid_t pid) {
        pid = pid ? pid : curr_pid;
        switch (reg_id) {
            case regs::pc:
                return register_map.at(pid).pc;
            case regs::sp:
                return register_map.at(pid).sp;
            case regs::pstate:
                return register_map.at(pid).pstate;
            default:
                return 0;
        }
    }

    void NCE::SetRegister(regs::sreg reg_id, uint32_t value, pid_t pid) {
        pid = pid ? pid : curr_pid;
        switch (reg_id) {
            case regs::pc:
                register_map.at(pid).pc = value;
            case regs::sp:
                register_map.at(pid).sp = value;
            case regs::pstate:
                register_map.at(pid).pstate = value;
        }
    }

    uint64_t MapSharedFunc(uint64_t address, size_t size, uint64_t perms, uint64_t fd) {
        return reinterpret_cast<uint64_t>(mmap(reinterpret_cast<void *>(address), size, static_cast<int>(perms), MAP_PRIVATE | MAP_ANONYMOUS | ((address) ? MAP_FIXED : 0), static_cast<int>(fd), 0)); // NOLINT(hicpp-signed-bitwise)
    }

    uint64_t NCE::MapShared(uint64_t address, size_t size, const memory::Permission perms) {
        int fd = -1;
        if(state && !state->os->process_map.empty()) {
            fd = ASharedMemory_create("", size);
            for(auto& process : state->os->process_map) {
                user_pt_regs fregs = {0};
                fregs.regs[0] = address;
                fregs.regs[1] = size;
                fregs.regs[2] = static_cast<uint64_t>(perms.get());
                fregs.regs[3] = static_cast<uint64_t>(fd);
                ExecuteFunction(reinterpret_cast<void *>(MapSharedFunc), fregs, process.second->main_thread);
                if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
                    throw exception("An error occurred while mapping shared region in child process");
                address = fregs.regs[0]; // In case address was 0, this ensures all processes allocate the same address
            }
        }
        void *ptr = mmap((void *) address, size, perms.get(), MAP_PRIVATE | MAP_ANONYMOUS | ((address) ? MAP_FIXED : 0), fd, 0); // NOLINT(hicpp-signed-bitwise)
        if (ptr == MAP_FAILED)
            throw exception(fmt::format("An occurred while mapping shared region: {}", strerror(errno)));
        addr_memory_map[address] = {address, size, perms, fd};
        return reinterpret_cast<uint64_t>(ptr);
    }

    uint64_t NCE::MapShared(uint64_t address, size_t size, const memory::Permission perms, const memory::Region region) {
        uint64_t addr = MapShared(address,size, perms); // TODO: Change MapShared return type to RegionData
        region_memory_map[region] = {addr, size, perms};
        return addr;
    }

    uint64_t RemapSharedFunc(uint64_t address, size_t old_size, size_t size) {
        return reinterpret_cast<uint64_t>(mremap(reinterpret_cast<void *>(address), old_size, size, 0));
    }

    void NCE::RemapShared(uint64_t address, size_t old_size, size_t size) {
        if(state && !state->os->process_map.empty()) {
            for(auto& process : state->os->process_map) {
                user_pt_regs fregs = {0};
                fregs.regs[0] = address;
                fregs.regs[1] = old_size;
                fregs.regs[2] = size;
                ExecuteFunction(reinterpret_cast<void *>(RemapSharedFunc), fregs, process.second->main_thread);
                if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
                    throw exception("An error occurred while remapping shared region in child process");
            }
        }
        void *ptr = mremap(reinterpret_cast<void *>(address), old_size, size, 0);
        if (ptr == MAP_FAILED)
            throw exception(fmt::format("An occurred while remapping shared region: {}", strerror(errno)));
        addr_memory_map.at(address).size = size;
    }

    void NCE::RemapShared(const memory::Region region, size_t size) {
        memory::RegionData& region_data = region_memory_map.at(region);
        RemapShared(region_data.address, region_data.size, size);
        region_data.size = size;
    }

    uint64_t UpdatePermissionSharedFunc(uint64_t address, size_t size, uint64_t perms) {
        return static_cast<uint64_t>(mprotect(reinterpret_cast<void *>(address), size, static_cast<int>(perms)));
    }

    void NCE::UpdatePermissionShared(uint64_t address, size_t size, const memory::Permission perms) {
        if(state && !state->os->process_map.empty()) {
            for(auto& process : state->os->process_map) {
                user_pt_regs fregs = {0};
                fregs.regs[0] = address;
                fregs.regs[1] = size;
                fregs.regs[2] = static_cast<uint64_t>(perms.get());
                ExecuteFunction(reinterpret_cast<void *>(UpdatePermissionSharedFunc), fregs, process.second->main_thread);
                if (static_cast<int>(fregs.regs[0]) == -1)
                    throw exception("An error occurred while updating shared region's permissions in child process");
            }
        }
        if (mprotect(reinterpret_cast<void *>(address), size, perms.get()) == -1)
            throw exception(fmt::format("An occurred while updating shared region's permissions: {}", strerror(errno)));
        addr_memory_map.at(address).perms = perms;
    }

    void NCE::UpdatePermissionShared(const memory::Region region, const memory::Permission perms) {
        memory::RegionData& region_data = region_memory_map.at(region);
        if (region_data.perms != perms) {
            UpdatePermissionShared(region_data.address, region_data.size, perms);
            region_data.perms = perms;
        }
    }

    uint64_t UnmapSharedFunc(uint64_t address, size_t size) {
        return static_cast<uint64_t>(munmap(reinterpret_cast<void *>(address), size));
    }

    void NCE::UnmapShared(uint64_t address, size_t size) {
        if(state && !state->os->process_map.empty()) {
            for(auto& process : state->os->process_map) {
                user_pt_regs fregs = {0};
                fregs.regs[0] = address;
                fregs.regs[1] = size;
                ExecuteFunction(reinterpret_cast<void *>(UnmapSharedFunc), fregs, process.second->main_thread);
                if (static_cast<int>(fregs.regs[0]) == -1)
                    throw exception("An error occurred while unmapping shared region in child process");
            }
        }
        int err = munmap(reinterpret_cast<void *>(address), size);
        if (err == -1)
            throw exception(fmt::format("An occurred while unmapping shared region: {}", strerror(errno)));
        if (addr_memory_map.at(address).fd!=-1) {
            if (close(addr_memory_map.at(address).fd)==-1)
                throw exception(fmt::format("An occurred while closing shared file descriptor: {}", strerror(errno)));
        }
    }

    void NCE::UnmapShared(const memory::Region region) {
        memory::RegionData& region_data = region_memory_map.at(region);
        UnmapShared(region_data.address, region_data.size);
        region_memory_map.erase(region);
    }
}
