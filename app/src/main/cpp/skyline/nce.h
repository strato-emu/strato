#pragma once

#include <syslog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <unordered_map>
#include "common.h"
#include "kernel/types/KSharedMemory.h"

namespace skyline {
    namespace instr {
        struct Brk;
    }
    /**
     * @brief The NCE (Native Code Execution) class is responsible for managing the state of catching instructions and directly controlling processes/threads
     */
    class NCE {
      private:
        pid_t currPid = 0; //!< The PID of the process currently being handled, this is so the PID won't have to be passed into functions like ReadRegister redundantly
        std::unordered_map<pid_t, user_pt_regs> registerMap; //!< A map of all PIDs and their corresponding registers (Whenever they were last updated)
        const DeviceState &state; //!< The state of the device

        /**
         * @brief Reads process registers into the `registers` variable
         * @param registers A set of registers to fill with values from the process
         * @param pid The PID of the process (Defaults to currPid)
         */
        void ReadRegisters(user_pt_regs &registers, pid_t pid = 0) const;

        /**
         * @brief Writes process registers from the `registers` variable
         * @param registers The registers to be written by the process
         * @param pid The PID of the process (Defaults to currPid)
         */
        void WriteRegisters(user_pt_regs &registers, pid_t pid = 0) const;

        /**
         * @brief Reads a BRK instruction, this is used to get it's immediate value
         * @param address The address of the BRK instruction
         * @param pid The PID of the process (Defaults to currPid)
         * @return An instance of BRK with the corresponding values
         */
        instr::Brk ReadBrk(u64 address, pid_t pid = 0) const;

      public:
        NCE(const DeviceState &state);

        /**
         * @brief Start the event loop of executing the program
         */
        void Execute();

        /**
         * @brief Execute any arbitrary function on a particular child process
         * @param func The entry point of the function
         * @param funcRegs A set of registers to run the function with (PC, SP and X29 are replaced)
         * @param pid The PID of the process
         */
        void ExecuteFunction(void *func, user_pt_regs &funcRegs, pid_t pid);

        /**
         * @brief Waits till a process calls "BRK #constant::brk_rdy"
         * @param pid The PID of the process
         * @return The registers after the BRK
         */
        user_pt_regs WaitRdy(pid_t pid);

        /**
         * @brief Pauses a particular process if was not already paused
         * @param pid The PID of the process (Defaults to currPid)
         * @return If the application was paused beforehand
         */
        bool PauseProcess(pid_t pid = 0) const;

        /**
         * @brief Resumes a particular process, does nothing if it was already running
         * @param pid The PID of the process (Defaults to currPid)
         */
        void ResumeProcess(pid_t pid = 0) const;

        /**
         * @brief Starts a particular process, sets the registers to their expected values and jumps to address
         * @param entryPoint The address to jump to
         * @param entryArg The argument to pass in for the entry function
         * @param stackTop The top of the stack
         * @param handle The handle of the main thread (Set to value of 1st register)
         * @param pid The PID of the process (Defaults to currPid)
         */
        void StartProcess(u64 entryPoint, u64 entryArg, u64 stackTop, u32 handle, pid_t pid) const;

        /**
         * @brief This prints out a trace and the CPU context
         * @param numHist The amount of previous instructions to print
         * @param pid The PID of the process (Defaults to currPid)
         */
        void ProcessTrace(u16 numHist = 10, pid_t pid = 0);

        /**
         * @brief Get the value of a Xn register
         * @param regId The ID of the register
         * @param pid The PID of the process (Defaults to currPid)
         * @return The value of the register
         */
        u64 GetRegister(Xreg regId, pid_t pid = 0);

        /**
         * @brief Set the value of a Xn register
         * @param regId The ID of the register
         * @param value The value to set
         * @param pid The PID of the process (Defaults to currPid)
         */
        void SetRegister(Xreg regId, u64 value, pid_t pid = 0);

        /**
         * @brief Get the value of a Wn register
         * @param regId The ID of the register
         * @param pid The PID of the process (Defaults to currPid)
         * @return The value in the register
         */
        u32 GetRegister(Wreg regId, pid_t pid = 0);

        /**
         * @brief Set the value of a Wn register
         * @param regId The ID of the register
         * @param value The value to set
         * @param pid The PID of the process (Defaults to currPid)
         */
        void SetRegister(Wreg regId, u32 value, pid_t pid = 0);

        /**
         * @brief Get the value of a special register
         * @param regId The ID of the register
         * @param pid The PID of the process (Defaults to currPid)
         * @return The value in the register
         */
        u64 GetRegister(Sreg regId, pid_t pid = 0);

        /**
         * @brief Set the value of a special register
         * @param regId The ID of the register
         * @param value The value to set
         * @param pid The PID of the process (Defaults to currPid)
         */
        void SetRegister(Sreg regId, u32 value, pid_t pid = 0);

        /**
         * @brief This patches specific parts of the code
         * @param code A vector with the code to be patched
         */
        std::vector<u32> PatchCode(std::vector<u8> &code, i64 offset);
    };
}
