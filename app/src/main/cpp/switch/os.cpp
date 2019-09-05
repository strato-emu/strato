#include "os.h"
#include "kernel/svc.h"
#include "loader/nro.h"
#include "nce.h"

extern bool halt;

namespace lightSwitch::kernel {
    OS::OS(std::shared_ptr<Logger> &logger, std::shared_ptr<Settings> &settings) : state(this, this_process, this_thread, std::make_shared<NCE>(), settings, logger) {}

    void OS::Execute(std::string rom_file) {
        std::string rom_ext = rom_file.substr(rom_file.find_last_of('.') + 1);
        std::transform(rom_ext.begin(), rom_ext.end(), rom_ext.begin(), [](unsigned char c) { return std::tolower(c); });

        if (rom_ext == "nro") loader::NroLoader loader(rom_file, state);
        else throw exception("Unsupported ROM extension.");

        auto main_process = CreateProcess(state.nce->region_memory_map.at(memory::Region::text).address, constant::def_stack_size);
        main_process->thread_map[main_process->main_thread]->Start(); // The kernel itself is responsible for starting the main thread
        state.nce->Execute(state);
    }

    /**
     * Function executed by all child processes after cloning
     */
    int ExecuteChild(void *) {
        ptrace(PTRACE_TRACEME);
        asm volatile("brk #0xFF"); // BRK #constant::brk_rdy (So we know when the thread/process is ready)
        return 0;
    }


    std::shared_ptr<type::KProcess> OS::CreateProcess(uint64_t address, size_t stack_size) {
        auto *stack = static_cast<uint8_t *>(mmap(nullptr, stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_NORESERVE | MAP_ANONYMOUS | MAP_STACK, -1, 0)); // NOLINT(hicpp-signed-bitwise)
        if (stack == MAP_FAILED)
            throw exception("Failed to allocate stack memory");
        if (mprotect(stack, PAGE_SIZE, PROT_NONE)) {
            munmap(stack, stack_size);
            throw exception("Failed to create guard pages");
        }
        pid_t pid = clone(&ExecuteChild, stack + stack_size, CLONE_FS | SIGCHLD, nullptr); // NOLINT(hicpp-signed-bitwise)
        if(pid==-1) throw exception(fmt::format("Call to clone() has failed: {}", strerror(errno)));
        std::shared_ptr<type::KProcess> process = std::make_shared<kernel::type::KProcess>(pid, address, reinterpret_cast<uint64_t>(stack), stack_size, state);
        process_map[pid] = process;
        state.logger->Write(Logger::DEBUG, "Successfully created process with PID: {}", pid);
        return process;
    }

    void OS::KillThread(pid_t pid) {
        auto process = process_map.at(pid);
        if(process->main_thread==pid) {
            state.logger->Write(Logger::DEBUG, "Exiting process with PID: {}", pid);
            // Erasing all shared_ptr instances to the process will call the destructor
            // However, in the case these are not all instances of it we wouldn't want to call the destructor
            for (auto& [key, value]: process->thread_map) {
                process_map.erase(key);
            };
        } else {
            state.logger->Write(Logger::DEBUG, "Exiting thread with TID: {}", pid);
            process->handle_table.erase(process->thread_map[pid]->handle);
            process->thread_map.erase(pid);
            process_map.erase(pid);
        }
    }

    void OS::SvcHandler(uint16_t svc, pid_t pid) {
        this_process = process_map.at(pid);
        this_thread = this_process->thread_map.at(pid);
        if (svc::svcTable[svc]) {
            state.logger->Write(Logger::DEBUG, "SVC called 0x{:X}", svc);
            (*svc::svcTable[svc])(state);
        } else {
            throw exception(fmt::format("Unimplemented SVC 0x{:X}", svc));
        }
    }

    ipc::IpcResponse OS::IpcHandler(ipc::IpcRequest &request) {
        ipc::IpcResponse response;
        switch (request.req_info->type) {
            case 4: // Request
                response.SetError(0xDEADBEE5);
                response.MoveHandle(0xBAADBEEF);
                response.MoveHandle(0xFACCF00D);
                response.Generate(state);
                break;
            default:
                throw exception(fmt::format("Unimplemented IPC message type {0}", uint16_t(request.req_info->type)));
        }
        return response;
    }
}
