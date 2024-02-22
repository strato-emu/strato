// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "KTransferMemory.h"
#include "KProcess.h"

namespace skyline::kernel::type {
    KTransferMemory::KTransferMemory(const DeviceState &state, size_t size)
        : KMemory{state, KType::KTransferMemory, size} {}

    u8 *KTransferMemory::Map(span<u8> map, memory::Permission permission) {
        // Get the host address of the guest memory
        auto hostMap{state.process->memory.GetHostSpan(map)};
        std::memcpy(host.data(), hostMap.data(), hostMap.size());
        u8 *result{KMemory::Map(map, permission)};

        auto oldChunk{state.process->memory.GetChunk(map.data()).value()};

        originalMapping = oldChunk.second;

        if (!originalMapping.state.transferMemoryAllowed) [[unlikely]] {
            LOGW("Tried to map transfer memory with incompatible state at: {} (0x{:X} bytes)", fmt::ptr(map.data()), map.size());
            return nullptr;
        } else {
            state.process->memory.MapTransferMemory(guest, permission);
            state.process->memory.SetRegionBorrowed(guest, true);
            return result;
        }
    }

    void KTransferMemory::Unmap(span<u8> map) {
        KMemory::Unmap(map);

        guest = span<u8>{};
        switch (originalMapping.state.type) {
            case memory::MemoryType::CodeMutable:
                state.process->memory.MapMutableCodeMemory(map);
                break;
            case memory::MemoryType::Heap:
                state.process->memory.MapHeapMemory(map);
                break;
            default:
                LOGW("Unmapping KTransferMemory with incompatible state: (0x{:X})", originalMapping.state.value);
        }
        map = state.process->memory.GetHostSpan(map);
        std::memcpy(map.data(), host.data(), map.size());
    }

    KTransferMemory::~KTransferMemory() {
        if (state.process && guest.valid()) {
            if (mmap(guest.data(), guest.size(), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS | MAP_POPULATE, -1, 0) == MAP_FAILED) [[unlikely]]
                LOGW("An error occurred while unmapping transfer memory in guest: {}", strerror(errno));

            switch (originalMapping.state.type) {
                case memory::MemoryType::CodeMutable:
                    state.process->memory.MapMutableCodeMemory(guest);
                    break;
                case memory::MemoryType::Heap:
                    state.process->memory.MapHeapMemory(guest);
                    break;
                default:
                    LOGW("Unmapping KTransferMemory with incompatible state: (0x{:X})", originalMapping.state.value);
            }
            std::memcpy(guest.data(), host.data(), guest.size());
        }
    }
}
