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
         * @return A span representing the memory object on the guest
         */
        virtual span<u8> Get() = 0;

        /**
         * @brief Updates the permissions of a block of mapped memory
         * @param ptr The starting address to change the permissions at
         * @param size The size of the partition to change the permissions of
         * @param permission The new permissions to be set for the memory
         */
        virtual void UpdatePermission(u8* ptr, size_t size, memory::Permission permission) = 0;

        bool IsInside(u8* ptr) {
            auto spn{Get()};
            return (spn.data() <= ptr) && ((spn.data() + spn.size()) > ptr);
        }

        virtual ~KMemory() = default;
    };
}
