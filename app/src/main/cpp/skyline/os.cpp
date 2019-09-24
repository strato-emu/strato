#include "os.h"
#include "kernel/svc.h"
#include "loader/nro.h"
#include "nce.h"

extern bool Halt;

namespace skyline::kernel {
    OS::OS(std::shared_ptr<Logger> &logger, std::shared_ptr<Settings> &settings) : state(this, thisProcess, thisThread, std::make_shared<NCE>(), settings, logger), serviceManager(state) {}

    void OS::Execute(const std::string &romFile) {
        state.nce->Initialize(state);
        std::string romExt = romFile.substr(romFile.find_last_of('.') + 1);
        std::transform(romExt.begin(), romExt.end(), romExt.begin(), [](unsigned char c) { return std::tolower(c); });

        if (romExt == "nro")
            loader::NroLoader loader(romFile, state);
        else
            throw exception("Unsupported ROM extension.");

        auto mainProcess = CreateProcess(state.nce->memoryRegionMap.at(memory::Region::Text)->address, constant::DefStackSize);
        mainProcess->threadMap.at(mainProcess->mainThread)->Start(); // The kernel itself is responsible for starting the main thread
        state.nce->Execute();
    }

    /**
       * Function executed by all child processes after cloning
       */
    int ExecuteChild(void *) {
        ptrace(PTRACE_TRACEME);
        asm volatile("Brk #0xFF"); // BRK #constant::brkRdy (So we know when the thread/process is ready)
        return 0;
    }

    std::shared_ptr<type::KProcess> OS::CreateProcess(u64 address, size_t stackSize) {
        auto *stack = static_cast<u8 *>(mmap(nullptr, stackSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_NORESERVE | MAP_ANONYMOUS | MAP_STACK, -1, 0)); // NOLINT(hicpp-signed-bitwise)
        if (stack == MAP_FAILED)
            throw exception("Failed to allocate stack memory");
        if (mprotect(stack, PAGE_SIZE, PROT_NONE)) {
            munmap(stack, stackSize);
            throw exception("Failed to create guard pages");
        }
        pid_t pid = clone(&ExecuteChild, stack + stackSize, CLONE_FS | SIGCHLD, nullptr); // NOLINT(hicpp-signed-bitwise)
        if (pid == -1)
            throw exception(fmt::format("Call to clone() has failed: {}", strerror(errno)));
        std::shared_ptr<type::KProcess> process = std::make_shared<kernel::type::KProcess>(0, pid, state, address, reinterpret_cast<u64>(stack), stackSize);
        threadMap[pid] = process;
        processVec.push_back(pid);
        state.logger->Write(Logger::Debug, "Successfully created process with PID: {}", pid);
        return process;
    }

    void OS::KillThread(pid_t pid) {
        auto process = threadMap.at(pid);
        if (process->mainThread == pid) {
            state.logger->Write(Logger::Debug, "Exiting process with PID: {}", pid);
            // Erasing all shared_ptr instances to the process will call the destructor
            // However, in the case these are not all instances of it we wouldn't want to call the destructor
            for (auto&[key, value]: process->threadMap)
                threadMap.erase(key);
            processVec.erase(std::remove(processVec.begin(), processVec.end(), pid), processVec.end());
        } else {
            state.logger->Write(Logger::Debug, "Exiting thread with TID: {}", pid);
            process->handleTable.erase(process->threadMap[pid]->handle);
            process->threadMap.erase(pid);
            threadMap.erase(pid);
        }
    }

    void OS::SvcHandler(u16 svc, pid_t pid) {
        if (svc::SvcTable[svc]) {
            state.logger->Write(Logger::Debug, "SVC called 0x{:X}", svc);
            (*svc::SvcTable[svc])(state);
        } else
            throw exception(fmt::format("Unimplemented SVC 0x{:X}", svc));
    }
}
