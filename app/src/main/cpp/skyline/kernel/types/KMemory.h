// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/memory.h>
#include "KObject.h"

namespace skyline::kernel::type {
    /**
     * @brief The base kernel memory object that other memory classes derieve from
     */
    class KMemory : public KObject {
      public:
        KMemory(const DeviceState &state, KType objectType) : KObject(state, objectType) {}

        /**
         * @brief Remap a chunk of memory as to change the size occupied by it
         * @param size The new size of the memory
         * @return The address the memory was remapped to
         */
        virtual void Resize(size_t size) = 0;

        /**
         * @brief Updates the permissions of a block of mapped memory
         * @param address The starting address to change the permissions at
         * @param size The size of the partition to change the permissions of
         * @param permission The new permissions to be set for the memory
         */
        virtual void UpdatePermission(const u64 address, const u64 size, memory::Permission permission) = 0;

        /**
         * @brief Updates the permissions of a chunk of mapped memory
         * @param permission The new permissions to be set for the memory
         */
        inline virtual void UpdatePermission(memory::Permission permission) = 0;

        /**
         * @brief Checks if the specified address is within the memory object
         * @param address The address to check
         * @return If the address is inside the memory object
         */
        inline virtual bool IsInside(u64 address) = 0;
    };
}
