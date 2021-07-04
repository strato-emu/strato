// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "KSharedMemory.h"

namespace skyline::kernel::type {
    /**
     * @brief KTransferMemory is used to transfer memory from one application to another on HOS, we emulate this abstraction using KSharedMemory as it's essentially the same with the main difference being that KSharedMemory is allocated by the kernel while KTransferMemory is created from memory that's been allocated by the guest beforehand
     */
    class KTransferMemory : public KSharedMemory {
      public:
        /**
         * @note 'ptr' needs to be in guest-reserved address space
         */
        KTransferMemory(const DeviceState &state, u8 *ptr, size_t size, memory::Permission permission, memory::MemoryState memState = memory::states::TransferMemory) : KSharedMemory(state, size, memState, KType::KTransferMemory) {
            std::memcpy(host.ptr, ptr, size);
            Map(ptr, size, permission);
        }
    };
}
