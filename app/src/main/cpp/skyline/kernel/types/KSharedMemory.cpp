// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/sharedmem.h>
#include <unistd.h>
#include <asm/unistd.h>
#include "KSharedMemory.h"
#include "KProcess.h"

namespace skyline::kernel::type {
    KSharedMemory::KSharedMemory(const DeviceState &state, size_t size, memory::MemoryState memState, KType type)
        : memoryState(memState),
          KMemory(state, type, span<u8>{}) {
        fd = ASharedMemory_create(type == KType::KSharedMemory ? "HOS-KSharedMemory" : "HOS-KTransferMemory", size);
        if (fd < 0)
            throw exception("An error occurred while creating shared memory: {}", fd);

        auto hostPtr{static_cast<u8 *>(mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd, 0))};
        if (hostPtr == MAP_FAILED)
            throw exception("An occurred while mapping shared memory: {}", strerror(errno));

        host = span<u8>{hostPtr, size};
    }

    u8 *KSharedMemory::Map(span<u8> map, memory::Permission permission) {
        if (!state.process->memory.AddressSpaceContains(map))
            throw exception("KPrivateMemory allocation isn't inside guest address space: 0x{:X} - 0x{:X}", map.data(), map.end().base());
        if (!util::IsPageAligned(map.data()) || !util::IsPageAligned(map.size()))
            throw exception("KSharedMemory mapping isn't page-aligned: 0x{:X} - 0x{:X} (0x{:X})", map.data(), map.end().base(), map.size());
        if (guest.valid())
            throw exception("Mapping KSharedMemory multiple times on guest is not supported: Requested Mapping: 0x{:X} - 0x{:X} (0x{:X}), Current Mapping: 0x{:X} - 0x{:X} (0x{:X})", map.data(), map.end().base(), map.size(), guest.data(), guest.end().base(), guest.size());

        auto guestPtr{static_cast<u8 *>(mmap(map.data(), map.size(), permission.Get(), MAP_SHARED | (map.data() ? MAP_FIXED : 0), fd, 0))};
        if (guestPtr == MAP_FAILED)
            throw exception("An error occurred while mapping shared memory in guest: {}", strerror(errno));
        guest = span<u8>{guestPtr, map.size()};

        state.process->memory.InsertChunk(ChunkDescriptor{
            .ptr = guest.data(),
            .size = guest.size(),
            .permission = permission,
            .state = memoryState,
            .attributes = memory::MemoryAttribute{
                .isBorrowed = objectType == KType::KTransferMemory,
            },
            .memory = this
        });

        return guest.data();
    }

    void KSharedMemory::Unmap(span<u8> map) {
        auto &memoryManager{state.process->memory};
        if (!memoryManager.AddressSpaceContains(map))
            throw exception("KPrivateMemory allocation isn't inside guest address space: 0x{:X} - 0x{:X}", map.data(), map.end().base());
        if (!util::IsPageAligned(map.data()) || !util::IsPageAligned(map.size()))
            throw exception("KSharedMemory mapping isn't page-aligned: 0x{:X} - 0x{:X} (0x{:X})", map.data(), map.end().base(), map.size());
        if (guest.data() != map.data() && guest.size() != map.size())
            throw exception("Unmapping KSharedMemory partially is not supported: Requested Unmap: 0x{:X} - 0x{:X} (0x{:X}), Current Mapping: 0x{:X} - 0x{:X} (0x{:X})", map.data(), map.end().base(), map.size(), guest.data(), guest.end().base(), guest.size());

        if (mmap(map.data(), map.size(), PROT_NONE, MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
            throw exception("An error occurred while unmapping shared memory in guest: {}", strerror(errno));

        guest = span<u8>{};
        memoryManager.InsertChunk(ChunkDescriptor{
            .ptr = map.data(),
            .size = map.size(),
            .state = memory::states::Unmapped,
        });
    }

    void KSharedMemory::UpdatePermission(span<u8> map, memory::Permission permission) {
        if (map.valid() && !util::IsPageAligned(map.data()))
            throw exception("KSharedMemory permission updated with a non-page-aligned address: 0x{:X}", map.data());

        if (guest.valid()) {
            mprotect(map.data(), map.size(), permission.Get());
            if (guest.data() == MAP_FAILED)
                throw exception("An error occurred while updating shared memory's permissions in guest: {}", strerror(errno));

            state.process->memory.InsertChunk(ChunkDescriptor{
                .ptr = map.data(),
                .size = map.size(),
                .permission = permission,
                .state = memoryState,
                .attributes = memory::MemoryAttribute{
                    .isBorrowed = objectType == KType::KTransferMemory,
                },
                .memory = this
            });
        }
    }

    KSharedMemory::~KSharedMemory() {
        if (state.process && guest.valid()) {
            auto &memoryManager{state.process->memory};
            if (objectType != KType::KTransferMemory) {
                if (mmap(guest.data(), guest.size(), PROT_NONE, MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
                    Logger::Warn("An error occurred while unmapping shared memory: {}", strerror(errno));

                state.process->memory.InsertChunk(ChunkDescriptor{
                    .ptr = guest.data(),
                    .size = guest.size(),
                    .state = memory::states::Unmapped,
                });
            } else {
                // KTransferMemory remaps the region with R/W permissions during destruction
                constexpr memory::Permission UnborrowPermission{true, true, false};

                if (mmap(guest.data(), guest.size(), UnborrowPermission.Get(), MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
                    Logger::Warn("An error occurred while remapping transfer memory: {}", strerror(errno));
                else if (!host.valid())
                    Logger::Warn("Expected host mapping of transfer memory to be valid during KTransferMemory destruction");
                guest.copy_from(host);

                state.process->memory.InsertChunk(ChunkDescriptor{
                    .ptr = guest.data(),
                    .size = guest.size(),
                    .permission = UnborrowPermission,
                    .state = memoryState,
                    .attributes = memory::MemoryAttribute{
                        .isBorrowed = false,
                    },
                    .memory = this
                });
            }
        }

        if (host.valid())
            munmap(host.data(), host.size());

        close(fd);
    }
}
