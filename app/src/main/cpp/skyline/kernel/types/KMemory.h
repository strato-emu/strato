// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/memory.h>
#include "KObject.h"

namespace skyline::kernel::type {
    /**
     * @brief The base kernel shared memory object that other memory classes derieve from
     */
    class KMemory : public KObject {
      private:
        int fd; //!< A file descriptor to the underlying shared memory

      public:
        KMemory(const DeviceState &state, KType objectType, size_t size);

        /**
         * @return A span representing the memory object on the guest
         */
        span<u8> guest;
        span<u8> host; //!< We also keep a host mirror of the underlying shared memory for host access, it is persistently mapped and should be used by anything accessing the memory on the host

        /**
         * @note 'ptr' needs to be in guest-reserved address space
         */
        virtual u8 *Map(span<u8> map, memory::Permission permission);

        /**
         * @note 'ptr' needs to be in guest-reserved address space
         */
        virtual void Unmap(span<u8> map);

        virtual ~KMemory();
    };
}
