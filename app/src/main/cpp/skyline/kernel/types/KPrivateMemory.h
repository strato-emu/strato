#pragma once

#include "KMemory.h"

namespace skyline::kernel::type {
    /**
     * @brief KPrivateMemory is used to map memory local to the guest process
     */
    class KPrivateMemory : public KMemory {
      private:
        int fd; //!< A file descriptor to the underlying shared memory

      public:
        u64 address{}; //!< The address of the allocated memory
        size_t size{}; //!< The size of the allocated memory

        /**
         * @param state The state of the device
         * @param address The address to map to (If NULL then an arbitrary address is picked)
         * @param size The size of the allocation
         * @param permission The permissions for the allocated memory
         * @param memState The MemoryState of the chunk of memory
         */
        KPrivateMemory(const DeviceState &state, u64 address, size_t size, memory::Permission permission, const memory::MemoryState memState);

        /**
         * @brief Remap a chunk of memory as to change the size occupied by it
         * @param size The new size of the memory
         * @return The address the memory was remapped to
         */
        virtual void Resize(size_t size);

        /**
         * @brief Updates the permissions of a block of mapped memory
         * @param address The starting address to change the permissions at
         * @param size The size of the partition to change the permissions of
         * @param permission The new permissions to be set for the memory
         */
        virtual void UpdatePermission(const u64 address, const u64 size, memory::Permission permission);

        /**
         * @brief Updates the permissions of a chunk of mapped memory
         * @param permission The new permissions to be set for the memory
         */
        inline virtual void UpdatePermission(memory::Permission permission) {
            UpdatePermission(address, size, permission);
        }

        /**
         * @brief Checks if the specified address is within the memory object
         * @param address The address to check
         * @return If the address is inside the memory object
         */
        inline virtual bool IsInside(u64 address) {
            return (this->address <= address) && ((this->address + this->size) > address);
        }

        /**
         * @brief The destructor of private memory, it deallocates the memory
         */
        ~KPrivateMemory();
    };
}
