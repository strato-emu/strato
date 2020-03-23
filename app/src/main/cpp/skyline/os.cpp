#include "os.h"
#include "loader/nro.h"
#include "nce/guest.h"

namespace skyline::kernel {
    OS::OS(std::shared_ptr<JvmManager> &jvmManager, std::shared_ptr<Logger> &logger, std::shared_ptr<Settings> &settings) : state(this, process, jvmManager, settings, logger), memory(state), serviceManager(state) {}

    void OS::Execute(const int romFd, const TitleFormat romType) {
        std::shared_ptr<loader::Loader> loader;
        if (romType == TitleFormat::NRO) {
            loader = std::make_shared<loader::NroLoader>(romFd);
        } else
            throw exception("Unsupported ROM extension.");
        auto process = CreateProcess(constant::BaseAddress, 0, constant::DefStackSize);
        loader->LoadProcessData(process, state);
        process->InitializeMemory();
        process->threads.at(process->pid)->Start(); // The kernel itself is responsible for starting the main thread
        state.nce->Execute();
    }

    std::shared_ptr<type::KProcess> OS::CreateProcess(u64 entry, u64 argument, size_t stackSize) {
        auto stack = std::make_shared<type::KSharedMemory>(state, 0, stackSize, memory::Permission{true, true, false}, memory::MemoryStates::Reserved, MAP_NORESERVE | MAP_STACK);
        stack->guest = stack->kernel;
        if (mprotect(reinterpret_cast<void *>(stack->guest.address), PAGE_SIZE, PROT_NONE))
            throw exception("Failed to create guard pages");
        auto tlsMem = std::make_shared<type::KSharedMemory>(state, 0, (sizeof(ThreadContext) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1), memory::Permission{true, true, false}, memory::MemoryStates::Reserved);
        tlsMem->guest = tlsMem->kernel;
        pid_t pid = clone(reinterpret_cast<int (*)(void *)>(&guest::GuestEntry), reinterpret_cast<void *>(stack->guest.address + stackSize), CLONE_FILES | CLONE_FS | CLONE_SETTLS | SIGCHLD, reinterpret_cast<void *>(entry), nullptr, reinterpret_cast<void *>(tlsMem->guest.address));
        if (pid == -1)
            throw exception("Call to clone() has failed: {}", strerror(errno));
        state.logger->Debug("Successfully created process with PID: {}", pid);
        process = std::make_shared<kernel::type::KProcess>(state, pid, argument, stack, tlsMem);
        return process;
    }

    void OS::KillThread(pid_t pid) {
        if (process->pid == pid) {
            state.logger->Debug("Killing process with PID: {}", pid);
            for (auto &thread: process->threads)
                thread.second->Kill();
        } else {
            state.logger->Debug("Killing thread with TID: {}", pid);
            process->threads.at(pid)->Kill();
        }
    }
}
