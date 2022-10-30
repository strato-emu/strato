// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "KMemory.h"

namespace skyline::kernel::type {
    /**
     * @brief KPrivateMemory is used to map memory local to the guest process
     * @note This does not reflect a kernel object in Horizon OS, it is an abstraction which makes things simpler to manage in Skyline instead
     */
    class KPrivateMemory : public KMemory {
      public:
        memory::Permission permission;
        memory::MemoryState memoryState;
        KHandle handle;

        /**
         * @param permission The permissions for the allocated memory (As reported to the application, host memory permissions aren't reflected by this)
         * @note 'ptr' needs to be in guest-reserved address space
         */
        KPrivateMemory(const DeviceState &state, KHandle handle, span<u8> guest, memory::Permission permission, memory::MemoryState memState);

        /**
         * @note There is no check regarding if any expansions will cause the memory mapping to leak into other mappings
         * @note Any extensions will have the same permissions and memory state as the initial mapping as opposed to extending the end
         */
        void Resize(size_t size);

        /**
         * @note This does not copy over anything, only contents of any overlapping regions will be retained
         */
        void Remap(span<u8> map);

        void UpdatePermission(span<u8> map, memory::Permission pPermission) override;

        /**
         * @brief The destructor of private memory, it deallocates the memory
         */
        ~KPrivateMemory();
    };
}
