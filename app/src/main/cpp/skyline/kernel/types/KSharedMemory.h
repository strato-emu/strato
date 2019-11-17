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
            memory::Permission permission;
        };
        std::unordered_map<pid_t, ProcessInfo> procInfMap; //!< Maps from a PID to where the memory was mapped to
        u16 ipcRefCount{}; //!< The amount of reference to this memory for IPC
        u16 deviceRefCount{};  //!< The amount of reference to this memory for IPC
        memory::Type type; //!< The type of this memory allocation

        /**
         * @param state The state of the device
         * @param address The address of the allocation on the kernel (Arbitrary is 0)
         * @param size The size of the allocation on the kernel
         * @param permission The permission of the kernel process
         * @param type The type of the memory
         */
        KSharedMemory(const DeviceState &state, u64 address, size_t size, const memory::Permission permission, memory::Type type);

        /**
         * @brief Maps the shared memory at an address
         * @param address The address to map to (If NULL an arbitrary address is picked)
         * @param size The amount of shared memory to map
         * @param permission The permission of the kernel process
         * @param pid The PID of the process
         * @return The address of the allocation
         */
        u64 Map(const u64 address, const u64 size, memory::Permission permission, pid_t pid);

        /**
         * @brief Resize a chunk of memory as to change the size occupied by it
         * @param newSize The new size of the memory
         */
        void Resize(size_t newSize);

        /**
         * Updates the permissions of a chunk of mapped memory
         * @param pid The PID of the requesting process
         * @param permission The new permissions to be set for the memory
         */
        void UpdatePermission(pid_t pid, memory::Permission permission);

        /**
         * @param pid The PID of the requesting process
         * @return A Memory::MemoryInfo struct based on attributes of the memory
         */
        memory::MemoryInfo GetInfo(pid_t pid);

        /**
         * @brief The destructor of shared memory, it deallocates the memory from all processes
         */
        ~KSharedMemory();
    };
}
