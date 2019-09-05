#pragma once

#include <syslog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <unordered_map>
#include "common.h"

namespace lightSwitch {
    class NCE {
    private:
        pid_t curr_pid = 0; //!< The PID of the process currently being handled, this is so the PID won't have to be passed into functions like ReadRegister redundantly
        std::unordered_map<pid_t, user_pt_regs> register_map; //!< A map of all PIDs and their corresponding registers (Whenever they were last updated)
        device_state *state;

        /**
         * Reads process registers into the `registers` variable
         * @param registers A set of registers to fill with values from the process
         * @param pid The PID of the process
         */
        void ReadRegisters(user_pt_regs &registers, pid_t pid=0) const;

        /**
         * Writes process registers from the `registers` variable
         * @param registers The registers to be written by the process
         * @param pid The PID of the process
         */
        void WriteRegisters(user_pt_regs &registers, pid_t pid=0) const;

        /**
         * @param address The address of the BRK instruction
         * @param pid The PID of the process
         * @return An instance of BRK with the corresponding values
         */
        instr::brk ReadBrk(uint64_t address, pid_t pid=0) const;

    public:
        std::map<memory::Region, memory::RegionData> region_memory_map; //!< A mapping from every memory::Region to it's corresponding memory::RegionData which holds it's address, size and fd
        std::map<uint64_t, memory::RegionData> addr_memory_map; //!< A mapping from every address to it's corresponding memory::RegionData

        /**
         * Iterates over shared_memory_vec unmapping all memory
         */
        ~NCE();

        /**
         * Start managing child processes
         * @param state The state of the device
         */
        void Execute(const device_state& state);

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
        void ResumeProcess(pid_t pid=0) const;

        /**
         * Starts a particular process, sets the registers to their expected values and jumps to address
         * @param address The address to jump to
         * @param entry_arg The argument to pass in for the entry function
         * @param stack_top The top of the stack
         * @param handle The handle of the main thread (Set to value of 1st register)
         * @param pid The PID of the process
         */
        void StartProcess(uint64_t address, uint64_t entry_arg, uint64_t stack_top, uint32_t handle, pid_t pid) const;

        /**
         * Get the value of a Xn register
         * @param reg_id The ID of the register
         * @param pid The PID of the process
         * @return The value of the register
         */
        uint64_t GetRegister(regs::xreg reg_id, pid_t pid=0);

        /**
         * Set the value of a Xn register
         * @param reg_id The ID of the register
         * @param value The value to set
         * @param pid The PID of the process
         */
        void SetRegister(regs::xreg reg_id, uint64_t value, pid_t pid=0);

        /**
         * Get the value of a Wn register
         * @param reg_id The ID of the register
         * @param pid The PID of the process
         * @return The value in the register
         */
        uint64_t GetRegister(regs::wreg reg_id, pid_t pid=0);

        /**
         * Set the value of a Wn register
         * @param reg_id The ID of the register
         * @param value The value to set
         * @param pid The PID of the process
         */
        void SetRegister(regs::wreg reg_id, uint32_t value, pid_t pid=0);

        /**
         * Get the value of a special register
         * @param reg_id The ID of the register
         * @param pid The PID of the process
         * @return The value in the register
         */
        uint64_t GetRegister(regs::sreg reg_id, pid_t pid=0);

        /**
         * Set the value of a special register
         * @param reg_id The ID of the register
         * @param value The value to set
         * @param pid The PID of the process
         */
        void SetRegister(regs::sreg reg_id, uint32_t value, pid_t pid=0);

        // TODO: Shared Memory mappings don't update after child process has been created
        /**
         * Map a chunk of shared memory
         * @param address The address to map to (Can be 0 if address doesn't matter)
         * @param size The size of the chunk of memory
         * @param perms The permissions of the memory
         * @return The address of the mapped chunk (Use when address is 0)
         */
        uint64_t MapShared(uint64_t address, size_t size, const memory::Permission perms);

        /**
         * Map a chunk of shared memory
         * @param address The address to map to (Can be 0 if address doesn't matter)
         * @param size The size of the chunk of memory
         * @param perms The permissions of the memory
         * @param region The specific region this memory is mapped for
         * @return The address of the mapped chunk (Use when address is 0)
         */
        uint64_t MapShared(uint64_t address, size_t size, const memory::Permission perms, const memory::Region region);

        /**
         * Remap a chunk of memory as to change the size occupied by it
         * @param address The address of the mapped memory
         * @param old_size The current size of the memory
         * @param size The new size of the memory
         */
        void RemapShared(uint64_t address, size_t old_size, size_t size);

        /**
         * Remap a chunk of memory as to change the size occupied by it
         * @param region The region of memory that was mapped
         * @param size The new size of the memory
         */
        void RemapShared(memory::Region region, size_t size);

        /**
         * Updates the permissions of a chunk of mapped memory
         * @param address The address of the mapped memory
         * @param size The size of the mapped memory
         * @param perms The new permissions to be set for the memory
         */
        void UpdatePermissionShared(uint64_t address, size_t size, const memory::Permission perms);

        /**
         * Updates the permissions of a chunk of mapped memory
         * @param region The region of memory that was mapped
         * @param perms The new permissions to be set for the memory
         */
        void UpdatePermissionShared(memory::Region region, memory::Permission perms);

        /**
         * Unmap a particular chunk of mapped memory
         * @param address The address of the mapped memory
         * @param size The size of the mapped memory
         */
        void UnmapShared(uint64_t address, size_t size);

        /**
         * Unmap a particular chunk of mapped memory
         * @param region The region of mapped memory
         */
        void UnmapShared(const memory::Region region);
    };
}
