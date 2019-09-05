#pragma once

#include <sys/mman.h>
#include "common.h"
#include "kernel/ipc.h"
#include "kernel/types/KProcess.h"
#include "kernel/types/KThread.h"
#include "nce.h"

namespace lightSwitch::kernel {
    /**
     * The OS class manages the interaction between OS components and the underlying hardware in NCE
     */
    class OS {
    private:
        device_state state; //!< The state of the device

    public:
        std::unordered_map<pid_t, std::shared_ptr<type::KProcess>> process_map; //!< The state of the device
        std::shared_ptr<type::KProcess> this_process; //!< The corresponding KProcess object of the process that's called an SVC
        std::shared_ptr<type::KThread> this_thread; //!< The corresponding KThread object of the thread that's called an SVC

        /**
         * @param logger An instance of the Logger class
         * @param settings An instance of the Settings class
         */
        OS(std::shared_ptr<Logger> &logger, std::shared_ptr<Settings> &settings);

        /**
         * Execute a particular ROM file. This launches a the main processes and calls the NCE class to handle execution.
         * @param rom_file The path to the ROM file to execute
         */
        void Execute(std::string rom_file);

        /**
         * Creates a new process
         * @param address The address of the initial function
         * @param stack_size The size of the main stack
         * @return An instance of the KProcess of the created process
         */
        std::shared_ptr<type::KProcess> CreateProcess(uint64_t address, size_t stack_size);

        /**
         * Kill a particular thread
         * @param pid The PID of the thread
         */
        void KillThread(pid_t pid);

        /**
         * @param svc The ID of the SVC to be called
         * @param pid The PID of the process/thread calling the SVC
         */
        void SvcHandler(uint16_t svc, pid_t pid);

        /**
         * @param req The IPC request to be handled
         * @return The corresponding response returned by a service
         */
        ipc::IpcResponse IpcHandler(ipc::IpcRequest &req);
    };
}
