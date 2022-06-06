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
        KMemory(const DeviceState &state, KType objectType, span <u8> guest) : KObject(state, objectType), guest(guest) {}

        /**
         * @return A span representing the memory object on the guest
         */
        span <u8> guest;

        /**
         * @brief Updates the permissions of a block of mapped memory
         * @param ptr The starting address to change the permissions at
         * @param size The size of the partition to change the permissions of
         * @param permission The new permissions to be set for the memory
         */
        virtual void UpdatePermission(span <u8> map, memory::Permission permission) = 0;

        virtual ~KMemory() = default;
    };
}
