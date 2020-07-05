// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "KMemory.h"

namespace skyline::kernel::type {
    /**
     * @brief KSharedMemory is used to hold a particular amount of shared memory
     */
    class KSharedMemory : public KMemory {
      private:
        int fd; //!< A file descriptor to the underlying shared memory
        memory::MemoryState initialState; //!< This is to hold the initial state for the Map call

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
            inline bool Valid() { return address && size && permission.Get(); }
        } kernel, guest;

        /**
         * @param state The state of the device
         * @param address The address of the allocation on the kernel (If NULL then an arbitrary address is picked)
         * @param size The size of the allocation on the kernel
         * @param permission The permission of the kernel process
         * @param memState The MemoryState of the chunk of memory
         * @param mmapFlags Additional flags to pass to mmap
         */
        KSharedMemory(const DeviceState &state, u64 address, size_t size, memory::Permission permission, memory::MemoryState memState = memory::states::SharedMemory, int mmapFlags = 0, bool shared = false);

        /**
         * @brief Maps the shared memory in the guest
         * @param address The address to map to (If NULL an arbitrary address is picked)
         * @param size The amount of shared memory to map
         * @param permission The permission of the kernel process
         * @return The address of the allocation
         */
        u64 Map(u64 address, u64 size, memory::Permission permission);

        /**
         * @brief Resize a chunk of memory as to change the size occupied by it
         * @param size The new size of the memory
         */
        virtual void Resize(size_t size);

        /**
         * @brief Updates the permissions of a block of mapped memory
         * @param address The starting address to change the permissions at
         * @param size The size of the partition to change the permissions of
         * @param permission The new permissions to be set for the memory
         * @param host Set the permissions for the kernel rather than the guest
         */
        void UpdatePermission(u64 address, u64 size, memory::Permission permission, bool host = false);

        /**
         * @brief Updates the permissions of a block of mapped memory
         * @param address The starting address to change the permissions at
         * @param size The size of the partition to change the permissions of
         * @param permission The new permissions to be set for the memory
         */
        virtual void UpdatePermission(u64 address, u64 size, memory::Permission permission) {
            UpdatePermission(address, size, permission, false);
        }

        /**
         * @brief Updates the permissions of a chunk of mapped memory
         * @param permission The new permissions to be set for the memory
         */
        inline virtual void UpdatePermission(memory::Permission permission) {
            UpdatePermission(guest.address, guest.size, permission, false);
        }

        /**
         * @brief Checks if the specified address is within the guest memory object
         * @param address The address to check
         * @return If the address is inside the guest memory object
         */
        inline virtual bool IsInside(u64 address) {
            return (guest.address <= address) && ((guest.address + guest.size) > address);
        }

        /**
         * @brief The destructor of shared memory, it deallocates the memory from all processes
         */
        ~KSharedMemory();
    };
}
