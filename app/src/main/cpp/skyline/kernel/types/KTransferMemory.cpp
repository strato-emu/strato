// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <asm/unistd.h>
#include <nce.h>
#include <os.h>
#include "KProcess.h"
#include "KTransferMemory.h"

namespace skyline::kernel::type {
    KTransferMemory::KTransferMemory(const DeviceState &state, u8* ptr, size_t size, memory::Permission permission, memory::MemoryState memState) : KSharedMemory(state, size, memState, KType::KTransferMemory) {
        std::memcpy(kernel.ptr, ptr, size);
        Map(ptr, size, permission);
    }
};
