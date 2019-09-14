#pragma once

#include "../../memory.h"
#include "KObject.h"

namespace lightSwitch::kernel::type {
    class KPrivateMemory {
      private:
        const device_state &state; //!< The state of the device

      public:
        u64 address; //!< The address of the allocated memory
        size_t size; //!< The size of the allocated memory
        u16 ipc_ref_count{}; //!< The amount of reference to this memory for IPC
        u16 device_ref_count{};  //!< The amount of reference to this memory for IPC
        Memory::Permission permission; //!< The amount of reference to this memory for IPC
        const Memory::Type type; //!< The type of this memory allocation
        const pid_t owner_pid; //!< The PID of the owner process

        /**
         * Constructor of a private memory object
         * @param dst_address The address to map to (If NULL then an arbitrary address is picked)
         * @param src_address The address to map from (If NULL then no copy is performed)
         * @param size The size of the allocation
         * @param permission The permissions for the memory
         * @param owner_pid The PID of the owner process
         */
        KPrivateMemory(const device_state &state, u64 dst_address, u64 src_address, size_t size, Memory::Permission permission, const Memory::Type type, const pid_t owner_pid);

        /**
         * Remap a chunk of memory as to change the size occupied by it
         * @param address The address of the mapped memory
         * @param old_size The current size of the memory
         * @param size The new size of the memory
         */
        void Resize(size_t new_size);

        /**
         * Updates the permissions of a chunk of mapped memory
         * @param perms The new permissions to be set for the memory
         */
        void UpdatePermission(Memory::Permission new_perms);

        /**
         * @param pid The PID of the requesting process
         * @return A Memory::MemoryInfo struct based on attributes of the memory
         */
        Memory::MemoryInfo GetInfo();
    };
}
