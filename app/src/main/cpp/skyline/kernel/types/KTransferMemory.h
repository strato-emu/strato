// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "KMemory.h"

namespace skyline::kernel::type {
    /**
     * @brief KTransferMemory is used to hold a particular amount of transferable memory
     */
    class KTransferMemory : public KMemory {
      private:
        ChunkDescriptor hostChunk{};
      public:
        bool host; //!< If the memory is mapped on the host or the guest
        u64 address; //!< The current address of the allocated memory for the kernel
        size_t size; //!< The current size of the allocated memory

        /**
         * @param state The state of the device
         * @param host If to map the memory on host or guest
         * @param address The address to map to (If NULL an arbitrary address is picked)
         * @param size The size of the allocation
         * @param permission The permissions of the memory
         * @param type The type of the memory
         * @param memState The MemoryState of the chunk of memory
         */
        KTransferMemory(const DeviceState &state, bool host, u64 address, size_t size, memory::Permission permission, memory::MemoryState memState = memory::states::TransferMemory);

        /**
         * @brief Transfers this piece of memory to another process
         * @param host If to transfer memory to host or guest
         * @param address The address to map to (If NULL an arbitrary address is picked)
         * @param size The amount of shared memory to map
         * @return The address of the allocation
         */
        u64 Transfer(bool host, u64 address, u64 size = 0);

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
        virtual void UpdatePermission(u64 address, u64 size, memory::Permission permission);

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
        ~KTransferMemory();
    };
}
