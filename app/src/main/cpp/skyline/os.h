#pragma once

#include <sys/mman.h>
#include <thread>
#include "common.h"
#include "kernel/ipc.h"
#include "kernel/types/KProcess.h"
#include "kernel/types/KThread.h"
#include "services/serviceman.h"
#include "nce.h"
#include "gpu.h"

namespace skyline::kernel {
    /**
     * @brief The OS class manages the interaction between Skyline components and the underlying OS in NCE
     */
    class OS {
      private:
        DeviceState state; //!< The state of the device

      public:
        std::unordered_map<pid_t, std::shared_ptr<type::KProcess>> processMap; //!< A mapping from a threat's PID to it's KProcess object
        std::vector<pid_t> processVec; //!< A vector of all processes by their main thread's PID
        std::shared_ptr<type::KProcess> thisProcess; //!< The corresponding KProcess object of the process that's called an SVC
        std::shared_ptr<type::KThread> thisThread; //!< The corresponding KThread object of the thread that's called an SVC
        service::ServiceManager serviceManager; //!< This manages all of the service functions

        /**
         * @param logger An instance of the Logger class
         * @param settings An instance of the Settings class
         */
        OS(std::shared_ptr<Logger> &logger, std::shared_ptr<Settings> &settings, ANativeWindow *window);

        /**
         * @brief Execute a particular ROM file. This launches a the main processes and calls the NCE class to handle execution.
         * @param romFile The path to the ROM file to execute
         */
        void Execute(const std::string &romFile);

        /**
         * @brief Creates a new process
         * @param address The address of the initial function
         * @param stackSize The size of the main stack
         * @return An instance of the KProcess of the created process
         */
        std::shared_ptr<type::KProcess> CreateProcess(u64 address, size_t stackSize);

        /**
         * @brief Kill a particular thread
         * @param pid The PID of the thread
         */
        void KillThread(pid_t pid);

        /**
         * @brief Handles a particular SuperVisor Call
         * @param svc The ID of the SVC to be called
         */
        void SvcHandler(u16 svc);

        /**
         * @brief Map a chunk of shared memory (Use only when kernel should be owner process else create KSharedMemory directly)
         * @param address The address to map to (Can be 0 if address doesn't matter)
         * @param size The size of the chunk of memory
         * @param localPermission The permissions of the memory for the kernel
         * @param remotePermission The permissions of the memory for the processes
         * @param type The type of the memory
         * @param region The specific region this memory is mapped for
         * @return A shared pointer to the kernel::type::KSharedMemory object
         */
        std::shared_ptr<kernel::type::KSharedMemory> MapSharedKernel(const u64 address, const size_t size, const memory::Permission kernelPermission, const memory::Permission remotePermission, const memory::Type type);
    };
}
