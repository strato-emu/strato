#pragma once

#include <syslog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <unordered_map>
#include "common.h"
#include "kernel/types/KSharedMemory.h"

namespace lightSwitch {
    class NCE {
      private:
        pid_t curr_pid = 0; //!< The PID of the process currently being handled, this is so the PID won't have to be passed into functions like ReadRegister redundantly
        std::unordered_map<pid_t, user_pt_regs> register_map; //!< A map of all PIDs and their corresponding registers (Whenever they were last updated)
        const device_state *state; //!< The state of the device

        /**
         * Reads process registers into the `registers` variable
         * @param registers A set of registers to fill with values from the process
         * @param pid The PID of the process
         */
        void ReadRegisters(user_pt_regs &registers, pid_t pid = 0) const;

        /**
         * Writes process registers from the `registers` variable
         * @param registers The registers to be written by the process
         * @param pid The PID of the process
         */
        void WriteRegisters(user_pt_regs &registers, pid_t pid = 0) const;

        /**
         * @param address The address of the BRK instruction
         * @param pid The PID of the process
         * @return An instance of BRK with the corresponding values
         */
        instr::brk ReadBrk(u64 address, pid_t pid = 0) const;

      public:
        std::map<Memory::Region, std::shared_ptr<kernel::type::KSharedMemory>> memory_region_map; //!< A mapping from every Memory::Region to a shared pointer to it's corresponding kernel::type::KSharedMemory
        std::map<u64, std::shared_ptr<kernel::type::KSharedMemory>> memory_map; //!< A mapping from every address to a shared pointer to it's corresponding kernel::type::KSharedMemory

        /**
         * Initialize NCE by setting the device_state variable
         * @param state The state of the device
         */
        void Initialize(const device_state &state);

        /**
         * Start managing child processes
         */
        void Execute();

        /**
         * Execute any arbitrary function on a particular child process
         * @param func The entry point of the function
         * @param func_regs A set of registers to run the function with (PC, SP and X29 are replaced)
         * @param pid The PID of the process
         */
        void ExecuteFunction(void *func, user_pt_regs &func_regs, pid_t pid);

        /**
         * Waits till a process calls "BRK #constant::brk_rdy"
         * @param pid The PID of the process
         * @return The registers after the BRK
         */
        user_pt_regs WaitRdy(pid_t pid);

        /**
         * Pauses a particular process if was not already paused
         * @param pid The PID of the process
         * @return If the application was paused beforehand
         */
        bool PauseProcess(pid_t pid = 0) const;

        /**
         * Resumes a particular process, does nothing if it was already running
         * @param pid The PID of the process
         */
        void ResumeProcess(pid_t pid = 0) const;

        /**
         * Starts a particular process, sets the registers to their expected values and jumps to address
         * @param address The address to jump to
         * @param entry_arg The argument to pass in for the entry function
         * @param stack_top The top of the stack
         * @param handle The handle of the main thread (Set to value of 1st register)
         * @param pid The PID of the process
         */
        void StartProcess(u64 address, u64 entry_arg, u64 stack_top, u32 handle, pid_t pid) const;

        /**
         * Get the value of a Xn register
         * @param reg_id The ID of the register
         * @param pid The PID of the process
         * @return The value of the register
         */
        u64 GetRegister(xreg reg_id, pid_t pid = 0);

        /**
         * Set the value of a Xn register
         * @param reg_id The ID of the register
         * @param value The value to set
         * @param pid The PID of the process
         */
        void SetRegister(xreg reg_id, u64 value, pid_t pid = 0);

        /**
         * Get the value of a Wn register
         * @param reg_id The ID of the register
         * @param pid The PID of the process
         * @return The value in the register
         */
        u64 GetRegister(wreg reg_id, pid_t pid = 0);

        /**
         * Set the value of a Wn register
         * @param reg_id The ID of the register
         * @param value The value to set
         * @param pid The PID of the process
         */
        void SetRegister(wreg reg_id, u32 value, pid_t pid = 0);

        /**
         * Get the value of a special register
         * @param reg_id The ID of the register
         * @param pid The PID of the process
         * @return The value in the register
         */
        u64 GetRegister(sreg reg_id, pid_t pid = 0);

        /**
         * Set the value of a special register
         * @param reg_id The ID of the register
         * @param value The value to set
         * @param pid The PID of the process
         */
        void SetRegister(sreg reg_id, u32 value, pid_t pid = 0);

        /**
         * Map a chunk of shared memory
         * @param address The address to map to (Can be 0 if address doesn't matter)
         * @param size The size of the chunk of memory
         * @param perms The permissions of the memory
         * @param type The type of the memory
         * @param region The specific region this memory is mapped for
         * @return A shared pointer to the kernel::type::KSharedMemory object
         */
        std::shared_ptr<kernel::type::KSharedMemory> MapSharedRegion(const u64 address, const size_t size, const Memory::Permission local_permission, const Memory::Permission remote_permission, const Memory::Type type, const Memory::Region region);

        /**
         * @return The total size of allocated shared memory
         */
        size_t GetSharedSize();
    };
}
