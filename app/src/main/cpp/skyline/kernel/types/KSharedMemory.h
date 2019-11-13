#pragma once

#include <memory.h>
#include "KObject.h"

namespace skyline::kernel::type {
    /**
     * @brief KSharedMemory is used to hold a particular amount of shared memory
     */
    class KSharedMemory : public KObject {
      private:
        int fd; //!< A file descriptor to the underlying shared memory

      public:
        /**
         * @brief This holds the address and size of a process's mapping
         */
        struct ProcessInfo {
            u64 address;
            size_t size;
        };
        std::unordered_map<pid_t, ProcessInfo> procInfMap; //!< Maps from a PID to where the memory was mapped to
        pid_t owner; //!< The PID of the process owning this shared memory
        u64 kaddress; //!< The address of the allocated memory for the kernel
        size_t ksize; //!< The size of the allocated memory
        u16 ipcRefCount{}; //!< The amount of reference to this memory for IPC
        u16 deviceRefCount{};  //!< The amount of reference to this memory for IPC
        memory::Permission localPermission; //!< The permission for the owner process
        memory::Permission remotePermission; //!< The permission of any process except the owner process
        memory::Type type; //!< The type of this memory allocation

        /**
         * @param state The state of the device
         * @param pid The PID of the owner thread
         * @param kaddress The address of the allocation on the kernel (Arbitrary is 0)
         * @param ksize The size of the allocation on the kernel
         * @param localPermission The permission of the owner process
         * @param remotePermission The permission of any process except the owner process
         * @param type The type of the memory
         */
        KSharedMemory(const DeviceState &state, pid_t pid, u64 kaddress, size_t ksize, const memory::Permission localPermission, const memory::Permission remotePermission, memory::Type type);

        /**
         * @brief Maps the shared memory at an address
         * @param address The address to map to (If NULL an arbitrary address is picked)
         * @param size The amount of shared memory to map
         * @param process The PID of the process
         * @return The address of the allocation
         */
        u64 Map(u64 address, u64 size, pid_t process);

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
         * @param process The PID of the requesting process
         * @return A Memory::MemoryInfo struct based on attributes of the memory
         */
        memory::MemoryInfo GetInfo(pid_t process);

        /**
         * @brief The destructor of shared memory, it deallocates the memory from all processes
         */
        ~KSharedMemory();
    };
}
