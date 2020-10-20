// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "KMemory.h"

namespace skyline::kernel::type {
    /**
     * @brief KPrivateMemory is used to map memory local to the guest process
     */
    class KPrivateMemory : public KMemory {
      public:
        u8* ptr{};
        size_t size{};
        memory::Permission permission;
        memory::MemoryState memState;

        /**
         * @param permission The permissions for the allocated memory (As reported to the application, host memory permissions aren't reflected by this)
         * @note 'ptr' needs to be in guest-reserved address space
         */
        KPrivateMemory(const DeviceState &state, u8* ptr, size_t size, memory::Permission permission, memory::MemoryState memState);

        void Resize(size_t size);

        /**
         * @note Only contents of any overlapping regions will be retained
         */
        void Remap(u8* ptr, size_t size);

        inline span<u8> Get() override {
            return span(ptr, size);
        }

        /**
         * @brief Updates the permissions of a block of mapped memory
         * @param address The starting address to change the permissions at
         * @param size The size of the partition to change the permissions of
         * @param permission The new permissions to be set for the memory
         */
        void UpdatePermission(u8* ptr, size_t size, memory::Permission permission) override;

        /**
         * @brief The destructor of private memory, it deallocates the memory
         */
        ~KPrivateMemory();
    };
}
