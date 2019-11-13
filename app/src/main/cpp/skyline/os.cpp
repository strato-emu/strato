#include "os.h"
#include "kernel/svc.h"
#include "loader/nro.h"

namespace skyline::kernel {
    OS::OS(std::shared_ptr<Logger> &logger, std::shared_ptr<Settings> &settings, ANativeWindow *window) : state(this, thisProcess, thisThread, window, settings, logger), serviceManager(state) {}

    void OS::Execute(const std::string &romFile) {
        std::string romExt = romFile.substr(romFile.find_last_of('.') + 1);
        std::transform(romExt.begin(), romExt.end(), romExt.begin(), [](unsigned char c) { return std::tolower(c); });
        auto process = CreateProcess(constant::BaseAddr, constant::DefStackSize);
        if (romExt == "nro") {
            loader::NroLoader loader(romFile);
            loader.LoadProcessData(process, state);
        } else
            throw exception("Unsupported ROM extension.");
        process->threadMap.at(process->mainThread)->Start(); // The kernel itself is responsible for starting the main thread
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
        pid_t pid = clone(&ExecuteChild, stack + stackSize, CLONE_FILES | CLONE_FS | SIGCHLD, nullptr); // NOLINT(hicpp-signed-bitwise)
        if (pid == -1)
            throw exception("Call to clone() has failed: {}", strerror(errno));
        std::shared_ptr<type::KProcess> process = std::make_shared<kernel::type::KProcess>(state, pid, address, reinterpret_cast<u64>(stack), stackSize);
        processMap[pid] = process;
        processVec.push_back(pid);
        state.logger->Debug("Successfully created process with PID: {}", pid);
        return process;
    }

    void OS::KillThread(pid_t pid) {
        auto process = processMap.at(pid);
        if (process->mainThread == pid) {
            state.logger->Debug("Exiting process with PID: {}", pid);
            // Erasing all shared_ptr instances to the process will call the destructor
            // However, in the case these are not all instances of it we wouldn't want to call the destructor
            for (auto&[key, value]: process->threadMap)
                processMap.erase(key);
            processVec.erase(std::remove(processVec.begin(), processVec.end(), pid), processVec.end());
        } else {
            state.logger->Debug("Exiting thread with TID: {}", pid);
            process->handleTable.erase(process->threadMap[pid]->handle);
            process->threadMap.erase(pid);
            processMap.erase(pid);
        }
    }

    void OS::SvcHandler(u16 svc) {
        if (svc::SvcTable[svc]) {
            state.logger->Debug("SVC called 0x{:X}", svc);
            (*svc::SvcTable[svc])(state);
        } else
            throw exception("Unimplemented SVC 0x{:X}", svc);
    }

    std::shared_ptr<kernel::type::KSharedMemory> OS::MapSharedKernel(const u64 address, const size_t size, const memory::Permission kernelPermission, const memory::Permission remotePermission, const memory::Type type) {
        return std::make_shared<kernel::type::KSharedMemory>(state, 0, address, size, kernelPermission, remotePermission, type);
    }
}
