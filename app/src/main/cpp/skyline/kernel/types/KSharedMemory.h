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
        struct MapInfo {
            u64 address;
            size_t size;
            memory::Permission permission;

            /**
             * @brief Returns if the object is valid
             * @return If the MapInfo object is valid
             */
            inline bool valid() { return address && size && permission.Get(); }
        } kernel, guest;

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
         * @brief Maps the shared memory at an address in the guest
         * @param address The address to map to (If NULL an arbitrary address is picked)
         * @param size The amount of shared memory to map
         * @param permission The permission of the kernel process
         * @return The address of the allocation
         */
        u64 Map(const u64 address, const u64 size, memory::Permission permission);

        /**
         * @brief Resize a chunk of memory as to change the size occupied by it
         * @param size The new size of the memory
         */
        void Resize(size_t size);

        /**
         * @brief Updates the permissions of a chunk of mapped memory
         * @param permission The new permissions to be set for the memory
         * @param kernel Set the permissions for the kernel rather than the guest
         */
        void UpdatePermission(memory::Permission permission, bool host = 0);

        /**
         * @brief Creates a MemoryInfo struct from the current instance
         * @return A Memory::MemoryInfo struct based on attributes of the memory
         */
        memory::MemoryInfo GetInfo();

        /**
         * @brief The destructor of shared memory, it deallocates the memory from all processes
         */
        ~KSharedMemory();
    };
}
