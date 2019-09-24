#pragma once

#include "../../memory.h"
#include "KObject.h"

namespace skyline::kernel::type {
    /**
     * @brief KSharedMemory is used to hold a particular amount of shared memory
     */
    class KSharedMemory : public KObject {
      private:
        int fd; //!< A file descriptor to the underlying shared memory

      public:
        u64 address{}; //!< The address of the allocated memory
        size_t size; //!< The size of the allocated memory
        u16 ipcRefCount{}; //!< The amount of reference to this memory for IPC
        u16 deviceRefCount{};  //!< The amount of reference to this memory for IPC
        memory::Permission localPermission; //!< The permission for the owner process
        memory::Permission remotePermission; //!< The permission of any process except the owner process
        memory::Type type; //!< The type of this memory allocation

        /**
         * @param handle A handle to this object
         * @param pid The PID of the main thread
         * @param state The state of the device
         * @param size The size of the allocation
         * @param localPermission The permission of the owner process
         * @param remotePermission The permission of any process except the owner process
         */
        KSharedMemory(handle_t handle, pid_t pid, const DeviceState &state, size_t size, const memory::Permission localPermission, const memory::Permission remotePermission, memory::Type type);

        /**
         * @brief Maps the shared memory at an address
         * @param address The address to map to (If NULL an arbitrary address is picked)
         */
        void Map(u64 address);

        /**
         * @brief Destructor of shared memory, it deallocates the memory from all processes
         */
        ~KSharedMemory();

        /**
         * @brief Resize a chunk of memory as to change the size occupied by it
         * @param newSize The new size of the memory
         */
        void Resize(size_t newSize);

        /**
         * Updates the permissions of a chunk of mapped memory
         * @param local If true change local permissions else change remote permissions
         * @param perms The new permissions to be set for the memory
         */
        void UpdatePermission(bool local, memory::Permission newPerms);

        /**
         * Initiates the instance of shared memory in a particular process
         * @param pid The PID of the process
         */
        void InitiateProcess(pid_t pid);

        /**
         * @param pid The PID of the requesting process
         * @return A Memory::MemoryInfo struct based on attributes of the memory
         */
        memory::MemoryInfo GetInfo(pid_t pid);
    };
}
