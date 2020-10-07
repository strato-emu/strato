// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nce.h"
#include "nce/guest.h"
#include "kernel/types/KProcess.h"
#include "vfs/os_backing.h"
#include "loader/nro.h"
#include "loader/nso.h"
#include "loader/nca.h"
#include "loader/nsp.h"
#include "os.h"

namespace skyline::kernel {
    OS::OS(std::shared_ptr<JvmManager> &jvmManager, std::shared_ptr<Logger> &logger, std::shared_ptr<Settings> &settings, const std::string &appFilesPath) : state(this, process, jvmManager, settings, logger), serviceManager(state), appFilesPath(appFilesPath) {}

    void OS::Execute(int romFd, loader::RomFormat romType) {
        auto romFile{std::make_shared<vfs::OsBacking>(romFd)};
        auto keyStore{std::make_shared<crypto::KeyStore>(appFilesPath)};

        if (romType == loader::RomFormat::NRO)
            state.loader = std::make_shared<loader::NroLoader>(romFile);
        else if (romType == loader::RomFormat::NSO)
            state.loader = std::make_shared<loader::NsoLoader>(romFile);
        else if (romType == loader::RomFormat::NCA)
            state.loader = std::make_shared<loader::NcaLoader>(romFile, keyStore);
        else if (romType == loader::RomFormat::NSP)
            state.loader = std::make_shared<loader::NspLoader>(romFile, keyStore);
        else
            throw exception("Unsupported ROM extension.");

        process = std::make_shared<kernel::type::KProcess>(state);
        state.loader->LoadProcessData(process, state);
        process->InitializeHeap();
        process->CreateThread(reinterpret_cast<void*>(constant::BaseAddress))->Start();

        state.nce->Execute();
    }

    void OS::KillThread(pid_t pid) {
        /*
        if (process->pid == pid) {
            state.logger->Debug("Killing process with PID: {}", pid);
            for (auto &thread: process->threads)
                thread.second->Kill();
        } else {
            state.logger->Debug("Killing thread with TID: {}", pid);
            process->threads.at(pid)->Kill();
        }
         */
    }
}
