#pragma once

#include "../../memory.h"
#include "KObject.h"

namespace lightSwitch::kernel::type {
    class KSharedMemory : public KObject {
      private:
        const device_state &state; //!< The state of the device
        int fd; //!< A file descriptor to the underlying shared memory

      public:
        u64 address; //!< The address of the allocated memory
        size_t size; //!< The size of the allocated memory
        u16 ipc_ref_count{}; //!< The amount of reference to this memory for IPC
        u16 device_ref_count{};  //!< The amount of reference to this memory for IPC
        Memory::Permission local_permission; //!< The amount of reference to this memory for IPC
        Memory::Permission remote_permission; //!< The permission of any process except the owner process
        Memory::Type type; //!< The type of this memory allocation
        pid_t owner_pid; //!< The PID of the owner process, 0 means memory is owned by kernel process

        /**
         * Constructor of a shared memory object
         * @param size The size of the allocation
         * @param local_permission The permission of the owner process
         * @param remote_permission The permission of any process except the owner process
         * @param owner_pid The PID of the owner process, 0 means memory is owned by kernel process
         */
        KSharedMemory(const device_state &state, size_t size, const Memory::Permission local_permission, const Memory::Permission remote_permission, Memory::Type type, handle_t handle, pid_t owner_pid = 0);

        /**
         * Maps the shared memory at an address
         * @param address The address to map to (If NULL an arbitrary address is picked)
         */
        void Map(u64 address);

        /**
         * Destructor of shared memory, it deallocates the memory from all processes
         */
        ~KSharedMemory();

        /**
         * Resize a chunk of memory as to change the size occupied by it
         * @param new_size The new size of the memory
         */
        void Resize(size_t new_size);

        /**
         * Updates the permissions of a chunk of mapped memory
         * @param local If true change local permissions else change remote permissions
         * @param perms The new permissions to be set for the memory
         */
        void UpdatePermission(bool local, Memory::Permission new_perms);

        /**
         * Initiates the instance of shared memory in a particular process
         * @param pid The PID of the process
         */
        void InitiateProcess(pid_t pid);

        /**
         * @param pid The PID of the requesting process
         * @return A Memory::MemoryInfo struct based on attributes of the memory
         */
        Memory::MemoryInfo GetInfo(pid_t pid);
    };
}
