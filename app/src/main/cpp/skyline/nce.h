#pragma once

#include <syslog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <unordered_map>
#include "common.h"
#include "kernel/types/KSharedMemory.h"

namespace skyline {
    /**
     * @brief The NCE (Native Code Execution) class is responsible for managing the state of catching instructions and directly controlling processes/threads
     */
    class NCE {
      private:
        DeviceState &state; //!< The state of the device
        std::unordered_map<pid_t, std::shared_ptr<std::thread>> threadMap; //!< This maps all of the host threads to their corresponding kernel thread

        /**
         * @brief This function is the event loop of a kernel thread managing a guest thread
         * @param thread The PID of the thread to manage
         */
        void KernelThread(pid_t thread);

      public:
        NCE(DeviceState &state);

        /**
         * @brief The destructor for NCE, this calls join() on all the threads
         */
        ~NCE();

        /**
         * @brief This function is the main event loop of the program
         */
        void Execute();

        /**
         * @brief Execute any arbitrary function on a specific child thread
         * @param call The specific call to execute
         * @param funcRegs A set of registers to run the function
         * @param thread The thread to execute the function on
         */
        void ExecuteFunction(ThreadCall call, Registers &funcRegs, std::shared_ptr<kernel::type::KThread> &thread);

        /**
         * @brief Execute any arbitrary function on the child process
         * @param call The specific call to execute
         * @param funcRegs A set of registers to run the function
         */
        void ExecuteFunction(ThreadCall call, Registers &funcRegs);

        /**
         * @brief Waits till a thread is ready to execute commands
         * @param thread The KThread to wait for initialization
         */
        void WaitThreadInit(std::shared_ptr<kernel::type::KThread> &thread);

        /**
         * @brief Sets the X0 and X1 registers in a thread and starts it and it's kernel thread
         * @param entryArg The argument to pass in for the entry function
         * @param handle The handle of the main thread
         * @param thread The thread to set the registers and start
         * @note This function will block forever if the thread has already started
         */
        void StartThread(u64 entryArg, u32 handle, std::shared_ptr<kernel::type::KThread> &thread);

        /**
         * @brief This prints out a trace and the CPU context
         * @param numHist The amount of previous instructions to print (Can be 0)
         * @param ctx The ThreadContext of the thread to log
         */
        void ThreadTrace(u16 numHist = 10, ThreadContext *ctx = nullptr);

        /**
         * @brief This patches specific parts of the code
         * @param code A vector with the code to be patched
         * @param baseAddress The address at which the code is mapped
         * @param offset The offset of the code block from the base address
         */
        std::vector<u32> PatchCode(std::vector<u8> &code, u64 baseAddress, i64 offset);
    };
}
