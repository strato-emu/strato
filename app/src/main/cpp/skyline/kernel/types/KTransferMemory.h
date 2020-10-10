// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "KSharedMemory.h"

namespace skyline::kernel::type {
    /**
     * @brief KTransferMemory is used to transfer memory from one application to another on HOS, we emulate this abstraction using KSharedMemory as it's functionally indistinguishable for the guest and allows access from the kernel regardless of if it's mapped on the guest
     */
    class KTransferMemory : public KSharedMemory {
      public:
        KTransferMemory(const DeviceState &state, u8 *ptr, size_t size, memory::Permission permission, memory::MemoryState memState = memory::states::TransferMemory) : KSharedMemory(state, size, memState, KType::KTransferMemory) {
            std::memcpy(kernel.ptr, ptr, size);
            Map(ptr, size, permission);
        }
    };
}
