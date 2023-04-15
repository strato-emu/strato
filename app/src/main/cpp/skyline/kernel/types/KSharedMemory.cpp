// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/sharedmem.h>
#include <unistd.h>
#include <asm/unistd.h>
#include "KSharedMemory.h"
#include "KProcess.h"

namespace skyline::kernel::type {
    KSharedMemory::KSharedMemory(const DeviceState &state, size_t size, KType type)
        : KMemory(state, type, span<u8>{}) {
        fd = ASharedMemory_create(type == KType::KSharedMemory ? "HOS-KSharedMemory" : "HOS-KTransferMemory", size);
        if (fd < 0) [[unlikely]]
            throw exception("An error occurred while creating shared memory: {}", fd);

        u8 *hostPtr{static_cast<u8 *>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0))};
        if (hostPtr == MAP_FAILED) [[unlikely]]
            throw exception("An occurred while mapping shared memory: {}", strerror(errno));

        host = span<u8>{hostPtr, size};
    }

    u8 *KSharedMemory::Map(span<u8> map, memory::Permission permission) {
        if (!state.process->memory.AddressSpaceContains(map)) [[unlikely]]
            throw exception("KSharedMemory allocation isn't inside guest address space: 0x{:X} - 0x{:X}", map.data(), map.end().base());
        if (!util::IsPageAligned(map.data()) || !util::IsPageAligned(map.size())) [[unlikely]]
            throw exception("KSharedMemory mapping isn't page-aligned: 0x{:X} - 0x{:X} (0x{:X})", map.data(), map.end().base(), map.size());
        if (guest.valid()) [[unlikely]]
            throw exception("Mapping KSharedMemory multiple times on guest is not supported: Requested Mapping: 0x{:X} - 0x{:X} (0x{:X}), Current Mapping: 0x{:X} - 0x{:X} (0x{:X})", map.data(), map.end().base(), map.size(), guest.data(), guest.end().base(), guest.size());

        auto guestPtr{static_cast<u8 *>(mmap(map.data(), map.size(), permission.Get(), MAP_SHARED | (map.data() ? MAP_FIXED : 0), fd, 0))};
        if (guestPtr == MAP_FAILED) [[unlikely]]
            throw exception("An error occurred while mapping shared memory in guest: {}", strerror(errno));
        guest = span<u8>{guestPtr, map.size()};

        if (objectType == KType::KTransferMemory) {
            state.process->memory.MapTransferMemory(guest, permission);
            state.process->memory.SetLockOnChunks(guest, true);
        } else {
            state.process->memory.MapSharedMemory(guest, permission);
        }

        return guest.data();
    }

    void KSharedMemory::Unmap(span<u8> map) {
        auto &memoryManager{state.process->memory};
        if (!memoryManager.AddressSpaceContains(map)) [[unlikely]]
            throw exception("KSharedMemory allocation isn't inside guest address space: 0x{:X} - 0x{:X}", map.data(), map.end().base());
        if (!util::IsPageAligned(map.data()) || !util::IsPageAligned(map.size())) [[unlikely]]
            throw exception("KSharedMemory mapping isn't page-aligned: 0x{:X} - 0x{:X} ({} bytes)", map.data(), map.end().base(), map.size());
        if (guest.data() != map.data() && guest.size() != map.size()) [[unlikely]]
            throw exception("Unmapping KSharedMemory partially is not supported: Requested Unmap: 0x{:X} - 0x{:X} (0x{:X}), Current Mapping: 0x{:X} - 0x{:X} (0x{:X})", map.data(), map.end().base(), map.size(), guest.data(), guest.end().base(), guest.size());

        if (mmap(map.data(), map.size(), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) [[unlikely]]
            throw exception("An error occurred while unmapping shared memory in guest: {}", strerror(errno));

        guest = span<u8>{};
        memoryManager.UnmapMemory(map);
    }

    KSharedMemory::~KSharedMemory() {
        if (state.process && guest.valid()) {
            auto &memoryManager{state.process->memory};

            if (mmap(guest.data(), guest.size(), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) [[unlikely]]
                Logger::Warn("An error occurred while unmapping shared memory: {}", strerror(errno));

            state.process->memory.UnmapMemory(guest);
        }

        if (host.valid())
            munmap(host.data(), host.size());

        close(fd);
    }
}
