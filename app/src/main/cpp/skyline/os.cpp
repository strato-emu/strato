// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nce.h"
#include "nce/guest.h"
#include "kernel/memory.h"
#include "kernel/types/KProcess.h"
#include "vfs/os_backing.h"
#include "loader/nro.h"
#include "loader/nso.h"
#include "loader/nca.h"
#include "loader/nsp.h"
#include "os.h"

namespace skyline::kernel {
    OS::OS(std::shared_ptr<JvmManager> &jvmManager, std::shared_ptr<Logger> &logger, std::shared_ptr<Settings> &settings, const std::string &appFilesPath) : state(this, process, jvmManager, settings, logger), memory(state), serviceManager(state), appFilesPath(appFilesPath) {}

    void OS::Execute(int romFd, loader::RomFormat romType) {
        auto romFile{std::make_shared<vfs::OsBacking>(romFd)};
        auto keyStore{std::make_shared<crypto::KeyStore>(appFilesPath)};

        if (romType == loader::RomFormat::NRO) {
            state.loader = std::make_shared<loader::NroLoader>(romFile);
        } else if (romType == loader::RomFormat::NSO) {
            state.loader = std::make_shared<loader::NsoLoader>(romFile);
        } else if (romType == loader::RomFormat::NCA) {
            state.loader = std::make_shared<loader::NcaLoader>(romFile, keyStore);
        } else if (romType == loader::RomFormat::NSP) {
            state.loader = std::make_shared<loader::NspLoader>(romFile, keyStore);
        } else {
            throw exception("Unsupported ROM extension.");
        }

        process = CreateProcess(constant::BaseAddress, 0, constant::DefStackSize);
        state.loader->LoadProcessData(process, state);
        process->InitializeMemory();
        process->threads.at(process->pid)->Start(); // The kernel itself is responsible for starting the main thread

        state.nce->Execute();
    }

    std::shared_ptr<type::KProcess> OS::CreateProcess(u64 entry, u64 argument, size_t stackSize) {
        auto stack{std::make_shared<type::KSharedMemory>(state, stackSize, memory::states::Stack)};
        stack->guest = stack->kernel;

        if (mprotect(stack->guest.ptr, PAGE_SIZE, PROT_NONE))
            throw exception("Failed to create guard pages");

        auto tlsMem{std::make_shared<type::KSharedMemory>(state, (sizeof(ThreadContext) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1), memory::states::Reserved)};
        tlsMem->guest = tlsMem->kernel;

        auto pid{clone(reinterpret_cast<int (*)(void *)>(&guest::GuestEntry), stack->guest.ptr + stackSize, CLONE_FILES | CLONE_FS | CLONE_SETTLS | SIGCHLD, reinterpret_cast<void *>(entry), nullptr, tlsMem->guest.ptr)};
        if (pid == -1)
            throw exception("Call to clone() has failed: {}", strerror(errno));

        state.logger->Debug("Successfully created process with PID: {}", pid);
        return std::make_shared<kernel::type::KProcess>(state, pid, entry, stack, tlsMem);
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
