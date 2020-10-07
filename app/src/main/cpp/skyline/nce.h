// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"
#include <sys/wait.h>

namespace skyline {
    /**
     * @brief The NCE (Native Code Execution) class is responsible for managing the state of catching instructions and directly controlling processes/threads
     */
    class NCE {
      private:
        DeviceState &state;
        std::unordered_map<pid_t, std::shared_ptr<std::thread>> threadMap; //!< This maps all of the host threads to their corresponding kernel thread

        /**
         * @brief The event loop of a kernel thread managing a guest thread
         * @param thread The PID of the thread to manage
         */
        void KernelThread(pid_t thread);

      public:
        NCE(DeviceState &state);

        ~NCE();

        void Execute();

        void SignalHandler(int signal, const siginfo &info, const ucontext &ucontext);

        /**
         * @brief A delegator to the real signal handler after restoring context
         */
        static void SignalHandler(int signal, siginfo *info, void *context);

        /**
         * @brief Prints out a trace and the CPU context
         * @param instructionCount The amount of previous instructions to print (Can be 0)
         */
        void ThreadTrace(u16 instructionCount = 10, ThreadContext *ctx = nullptr);

        /**
         * @brief Patches specific parts of the code
         * @param code A vector with the code to be patched
         * @param baseAddress The address at which the code is mapped
         * @param offset The offset of the code block from the base address
         */
        std::vector<u32> PatchCode(std::vector<u8> &code, u64 baseAddress, i64 offset);
    };
}
