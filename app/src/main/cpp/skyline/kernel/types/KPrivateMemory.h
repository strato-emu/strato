#pragma once

#include <memory.h>
#include "KObject.h"

namespace skyline::kernel::type {
    /**
     * KPrivateMemory is used to hold some amount of private memory
     */
    class KPrivateMemory : public KObject {
      private:
        const DeviceState &state; //!< The state of the device

      public:
        pid_t owner; //!< The PID of the process owning this memory
        u64 address; //!< The address of the allocated memory
        size_t size; //!< The size of the allocated memory
        u16 ipcRefCount{}; //!< The amount of reference to this memory for IPC
        u16 deviceRefCount{};  //!< The amount of reference to this memory for IPC
        memory::Permission permission; //!< The permissions for the allocated memory
        const memory::Type type; //!< The type of this memory allocation
        std::vector<memory::RegionInfo> regionInfoVec; //!< This holds information about specific memory regions

        /**
         * @param state The state of the device
         * @param pid The PID of the main
         * @param dstAddress The address to map to (If NULL then an arbitrary address is picked)
         * @param srcAddress The address to map from (If NULL then no copy is performed)
         * @param size The size of the allocation
         * @param permission The permissions for the allocated memory
         * @param type The type of the memory
         */
        KPrivateMemory(const DeviceState &state, pid_t pid, u64 dstAddress, u64 srcAddress, size_t size, memory::Permission permission, const memory::Type type);

        /**
         * @brief Remap a chunk of memory as to change the size occupied by it
         * @param newSize The new size of the memory
         * @param canMove If the memory can move if there is not enough space at the current address
         * @return The address the memory was remapped to
         */
        u64 Resize(size_t newSize, bool canMove);

        /**
         * @brief Updates the permissions of a chunk of mapped memory
         * @param permission The new permissions to be set for the memory
         */
        void UpdatePermission(memory::Permission permission);

        /**
         * @brief Returns a MemoryInfo object
         * @param address The specific address being queried (Used to fill MemoryAttribute)
         * @return A Memory::MemoryInfo struct based on attributes of the memory
         */
        memory::MemoryInfo GetInfo(u64 address);

        /**
         * @brief The destructor of private memory, it deallocates the memory
         */
        ~KPrivateMemory();
    };
}
