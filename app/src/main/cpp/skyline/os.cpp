#include "os.h"
#include "loader/nro.h"
#include "nce/guest.h"

namespace skyline::kernel {
    OS::OS(std::shared_ptr<JvmManager> &jvmManager, std::shared_ptr<Logger> &logger, std::shared_ptr<Settings> &settings) : state(this, process, jvmManager, settings, logger), serviceManager(state) {}

    void OS::Execute(const int romFd, const TitleFormat romType) {
        std::shared_ptr<loader::Loader> loader;
        if (romType == TitleFormat::NRO) {
            loader = std::make_shared<loader::NroLoader>(romFd);
        } else
            throw exception("Unsupported ROM extension.");
        auto process = CreateProcess(loader->mainEntry, 0, constant::DefStackSize);
        loader->LoadProcessData(process, state);
        process->threadMap.at(process->pid)->Start(); // The kernel itself is responsible for starting the main thread
        state.nce->Execute();
    }

    std::shared_ptr<type::KProcess> OS::CreateProcess(u64 entry, u64 argument, size_t stackSize) {
        auto *stack = static_cast<u8 *>(mmap(nullptr, stackSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_NORESERVE | MAP_ANONYMOUS | MAP_STACK, -1, 0));
        if (stack == MAP_FAILED)
            throw exception("Failed to allocate stack memory");
        if (mprotect(stack, PAGE_SIZE, PROT_NONE)) {
            munmap(stack, stackSize);
            throw exception("Failed to create guard pages");
        }
        auto tlsMem = std::make_shared<type::KSharedMemory>(state, 0, (sizeof(ThreadContext) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1), memory::Permission(true, true, false), memory::Type::Reserved);
        tlsMem->guest = tlsMem->kernel;
        pid_t pid = clone(reinterpret_cast<int (*)(void *)>(&guest::entry), stack + stackSize, CLONE_FILES | CLONE_FS | CLONE_SETTLS | SIGCHLD, reinterpret_cast<void *>(entry), nullptr, reinterpret_cast<void *>(tlsMem->guest.address));
        if (pid == -1)
            throw exception("Call to clone() has failed: {}", strerror(errno));
        state.logger->Debug("Successfully created process with PID: {}", pid);
        process = std::make_shared<kernel::type::KProcess>(state, pid, argument, reinterpret_cast<u64>(stack), stackSize, tlsMem);
        state.logger->Debug("Successfully created process with PID: {}", pid);
        return process;
    }

    void OS::KillThread(pid_t pid) {
        if (process->pid == pid) {
            state.logger->Debug("Killing process with PID: {}", pid);
            for (auto &thread: process->threadMap)
                thread.second->Kill();
        } else {
            state.logger->Debug("Killing thread with TID: {}", pid);
            process->threadMap.at(pid)->Kill();
        }
    }
}
