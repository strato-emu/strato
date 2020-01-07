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
        u64 address; //!< The address of the allocated memory
        size_t size; //!< The size of the allocated memory
        u16 ipcRefCount{}; //!< The amount of reference to this memory for IPC
        u16 deviceRefCount{};  //!< The amount of reference to this memory for IPC
        memory::Permission permission; //!< The permissions for the allocated memory
        const memory::Type type; //!< The type of this memory allocation
        std::vector<memory::RegionInfo> regionInfoVec; //!< This holds information about specific memory regions

        /**
         * @param state The state of the device
         * @param dstAddress The address to map to (If NULL then an arbitrary address is picked)
         * @param size The size of the allocation
         * @param permission The permissions for the allocated memory
         * @param type The type of the memory
         * @param thread The thread to execute the calls on
         */
        KPrivateMemory(const DeviceState &state, u64 dstAddress, size_t size, memory::Permission permission, const memory::Type type, std::shared_ptr<KThread> thread = 0);

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
