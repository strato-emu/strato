#pragma once

#include <memory.h>
#include "KObject.h"

namespace skyline::kernel::type {
    /**
     * @brief KTransferMemory is used to hold a particular amount of transferable memory
     */
    class KTransferMemory : public KObject {
      public:
        pid_t owner; //!< The PID of the process owning this memory
        u64 cAddress; //!< The current address of the allocated memory for the kernel
        size_t cSize; //!< The current size of the allocated memory
        u16 ipcRefCount{}; //!< The amount of reference to this memory for IPC
        u16 deviceRefCount{};  //!< The amount of reference to this memory for IPC
        memory::Permission permission; //!< The permissions of the memory

        /**
         * @param state The state of the device
         * @param pid The PID of the owner thread (Use 0 for kernel)
         * @param address The address to map to (If NULL an arbitrary address is picked)
         * @param size The size of the allocation
         * @param permission The permissions of the memory
         * @param type The type of the memory
         */
        KTransferMemory(const DeviceState &state, pid_t pid, u64 address, size_t size, const memory::Permission permission);

        /**
         * @brief Transfers this piece of memory to another process
         * @param process The PID of the process (Use 0 for kernel)
         * @param address The address to map to (If NULL an arbitrary address is picked)
         * @param size The amount of shared memory to map
         * @return The address of the allocation
         */
        u64 Transfer(pid_t process, u64 address, u64 size);

        /**
         * @brief Returns a MemoryInfo struct filled with attributes of this region of memory
         * @return A memory::MemoryInfo struct based on attributes of the memory
         */
        memory::MemoryInfo GetInfo();

        /**
         * @brief The destructor of private memory, it deallocates the memory
         */
        ~KTransferMemory();
    };
}
