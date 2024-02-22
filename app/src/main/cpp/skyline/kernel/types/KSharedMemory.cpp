// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "KSharedMemory.h"
#include "KProcess.h"

namespace skyline::kernel::type {
    KSharedMemory::KSharedMemory(const DeviceState &state, size_t size)
        : KMemory{state, KType::KSharedMemory, size} {}

    u8 *KSharedMemory::Map(span<u8> map, memory::Permission permission) {
        u8 *result{KMemory::Map(map, permission)};

        state.process->memory.MapSharedMemory(guest, permission);

        return result;
    }

    void KSharedMemory::Unmap(span<u8> map) {
        KMemory::Unmap(map);

        guest = span<u8>{};
        state.process->memory.UnmapMemory(map);
    }

    KSharedMemory::~KSharedMemory() {
        if (state.process && guest.valid()) {
            auto hostMap{state.process->memory.GetHostSpan(guest)};
            if (mmap(hostMap.data(), hostMap.size(), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) [[unlikely]]
                LOGW("An error occurred while unmapping shared memory: {}", strerror(errno));

            state.process->memory.UnmapMemory(guest);
        }
    }
}
