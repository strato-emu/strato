// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <thread>
#include <sys/mman.h>
#include "common.h"
#include "kernel/ipc.h"
#include "kernel/types/KProcess.h"
#include "kernel/types/KThread.h"
#include "services/serviceman.h"
#include "gpu.h"

namespace skyline::kernel {
    /**
     * @brief The OS class manages the interaction between Skyline components and the underlying OS in NCE
     */
    class OS {
      private:
        DeviceState state; //!< The state of the device

      public:
        std::shared_ptr<type::KProcess> process; //!< The KProcess object for the emulator, representing the guest process
        service::ServiceManager serviceManager; //!< This manages all of the service functions
        MemoryManager memory; //!< The MemoryManager object for this process

        /**
         * @param logger An instance of the Logger class
         * @param settings An instance of the Settings class
         * @param window The ANativeWindow object to draw the screen to
         */
        OS(std::shared_ptr<JvmManager> &jvmManager, std::shared_ptr<Logger> &logger, std::shared_ptr<Settings> &settings);

        /**
         * @brief Execute a particular ROM file. This launches the main process and calls the NCE class to handle execution.
         * @param romFd A FD to the ROM file to execute
         * @param romType The type of the ROM file
         */
        void Execute(const int romFd, const TitleFormat romType);

        /**
         * @brief Creates a new process
         * @param entry The entry point for the new process
         * @param argument The argument for the initial function
         * @param stackSize The size of the main stack
         * @return An instance of the KProcess of the created process
         */
        std::shared_ptr<type::KProcess> CreateProcess(u64 entry, u64 argument, size_t stackSize);

        /**
         * @brief Kill a particular thread
         * @param pid The PID of the thread
         */
        void KillThread(pid_t pid);
    };
}
