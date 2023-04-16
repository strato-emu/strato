// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/sharedmem.h>
#include <unistd.h>
#include <asm/unistd.h>
#include "KMemory.h"
#include "KProcess.h"

namespace skyline::kernel::type {
    KMemory::KMemory(const DeviceState &state, KType objectType, size_t size) : KObject(state, objectType), guest() {
        fd = ASharedMemory_create(objectType == KType::KSharedMemory ? "HOS-KSharedMemory" : "HOS-KTransferMemory", size);
        if (fd < 0) [[unlikely]]
            throw exception("An error occurred while creating shared memory: {}", fd);

        u8 *hostPtr{static_cast<u8 *>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0))};
        if (hostPtr == MAP_FAILED) [[unlikely]]
            throw exception("An occurred while mapping shared memory: {}", strerror(errno));

        host = span<u8>{hostPtr, size};
    }

    u8 *KMemory::Map(span<u8> map, memory::Permission permission) {
        if (!state.process->memory.AddressSpaceContains(map)) [[unlikely]]
            throw exception("KMemory allocation isn't inside guest address space: 0x{:X} - 0x{:X}", map.data(), map.end().base());
        if (!util::IsPageAligned(map.data()) || !util::IsPageAligned(map.size())) [[unlikely]]
            throw exception("KMemory mapping isn't page-aligned: 0x{:X} - 0x{:X} (0x{:X})", map.data(), map.end().base(), map.size());
        if (guest.valid()) [[unlikely]]
            throw exception("Mapping KMemory multiple times on guest is not supported: Requested Mapping: 0x{:X} - 0x{:X} (0x{:X}), Current Mapping: 0x{:X} - 0x{:X} (0x{:X})", map.data(), map.end().base(), map.size(), guest.data(), guest.end().base(), guest.size());

        if (mmap(map.data(), map.size(), permission.Get() ? PROT_READ | PROT_WRITE | PROT_EXEC : PROT_NONE, MAP_SHARED | (map.data() ? MAP_FIXED : 0), fd, 0) == MAP_FAILED) [[unlikely]]
            throw exception("An error occurred while mapping shared memory in guest: {}", strerror(errno));
        guest = map;

        return guest.data();
    }

    void KMemory::Unmap(span<u8> map) {
        if (!state.process->memory.AddressSpaceContains(map)) [[unlikely]]
            throw exception("KMemory allocation isn't inside guest address space: 0x{:X} - 0x{:X}", map.data(), map.end().base());
        if (!util::IsPageAligned(map.data()) || !util::IsPageAligned(map.size())) [[unlikely]]
            throw exception("KMemory mapping isn't page-aligned: 0x{:X} - 0x{:X} ({} bytes)", map.data(), map.end().base(), map.size());
        if (guest.data() != map.data() && guest.size() != map.size()) [[unlikely]]
            throw exception("Unmapping KMemory partially is not supported: Requested Unmap: 0x{:X} - 0x{:X} (0x{:X}), Current Mapping: 0x{:X} - 0x{:X} (0x{:X})", map.data(), map.end().base(), map.size(), guest.data(), guest.end().base(), guest.size());

        if (mmap(map.data(), map.size(), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) [[unlikely]]
            throw exception("An error occurred while unmapping shared/transfer memory in guest: {}", strerror(errno));
    }

    KMemory::~KMemory() {
        if (host.valid())
            munmap(host.data(), host.size());

        close(fd);
    }
}