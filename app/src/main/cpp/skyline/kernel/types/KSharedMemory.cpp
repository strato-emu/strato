// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/sharedmem.h>
#include <unistd.h>
#include <asm/unistd.h>
#include "KSharedMemory.h"
#include "KProcess.h"

namespace skyline::kernel::type {
    KSharedMemory::KSharedMemory(const DeviceState &state, size_t size, memory::MemoryState memState, KType type) : memoryState(memState), KMemory(state, type) {
        fd = ASharedMemory_create("KSharedMemory", size);
        if (fd < 0)
            throw exception("An error occurred while creating shared memory: {}", fd);

        host.ptr = static_cast<u8 *>(mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd, 0));
        if (host.ptr == MAP_FAILED)
            throw exception("An occurred while mapping shared memory: {}", strerror(errno));

        host.size = size;
    }

    u8 *KSharedMemory::Map(u8 *ptr, u64 size, memory::Permission permission) {
        if (!state.process->memory.base.IsInside(ptr) || !state.process->memory.base.IsInside(ptr + size))
            throw exception("KPrivateMemory allocation isn't inside guest address space: 0x{:X} - 0x{:X}", ptr, ptr + size);
        if (!util::PageAligned(ptr) || !util::PageAligned(size))
            throw exception("KSharedMemory mapping isn't page-aligned: 0x{:X} - 0x{:X} (0x{:X})", ptr, ptr + size, size);
        if (guest.Valid())
            throw exception("Mapping KSharedMemory multiple times on guest is not supported: Requested Mapping: 0x{:X} - 0x{:X} (0x{:X}), Current Mapping: 0x{:X} - 0x{:X} (0x{:X})", ptr, ptr + size, size, guest.ptr, guest.ptr + guest.size, guest.size);

        guest.ptr = static_cast<u8 *>(mmap(ptr, size, permission.Get(), MAP_SHARED | (ptr ? MAP_FIXED : 0), fd, 0));
        if (guest.ptr == MAP_FAILED)
            throw exception("An error occurred while mapping shared memory in guest: {}", strerror(errno));
        guest.size = size;

        state.process->memory.InsertChunk(ChunkDescriptor{
            .ptr = guest.ptr,
            .size = size,
            .permission = permission,
            .state = memoryState,
            .attributes = memory::MemoryAttribute{
                .isBorrowed = objectType == KType::KTransferMemory,
            },
        });

        return guest.ptr;
    }

    void KSharedMemory::Unmap(u8 *ptr, u64 size) {
        if (!state.process->memory.base.IsInside(ptr) || !state.process->memory.base.IsInside(ptr + size))
            throw exception("KPrivateMemory allocation isn't inside guest address space: 0x{:X} - 0x{:X}", ptr, ptr + size);
        if (!util::PageAligned(ptr) || !util::PageAligned(size))
            throw exception("KSharedMemory mapping isn't page-aligned: 0x{:X} - 0x{:X} (0x{:X})", ptr, ptr + size, size);
        if (guest.ptr != ptr && guest.size != size)
            throw exception("Unmapping KSharedMemory partially is not supported: Requested Unmap: 0x{:X} - 0x{:X} (0x{:X}), Current Mapping: 0x{:X} - 0x{:X} (0x{:X})", ptr, ptr + size, size, guest.ptr, guest.ptr + guest.size, guest.size);

        if (mmap(ptr, size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, 0, 0) == MAP_FAILED)
            throw exception("An error occurred while unmapping shared memory in guest: {}", strerror(errno));

        guest = {};
        state.process->memory.InsertChunk(ChunkDescriptor{
            .ptr = ptr,
            .size = size,
            .state = memory::states::Unmapped,
        });
    }

    void KSharedMemory::UpdatePermission(u8 *ptr, size_t size, memory::Permission permission) {
        if (ptr && !util::PageAligned(ptr))
            throw exception("KSharedMemory permission updated with a non-page-aligned address: 0x{:X}", ptr);

        if (guest.Valid()) {
            mprotect(ptr, size, permission.Get());
            if (guest.ptr == MAP_FAILED)
                throw exception("An error occurred while updating shared memory's permissions in guest: {}", strerror(errno));

            state.process->memory.InsertChunk(ChunkDescriptor{
                .ptr = ptr,
                .size = size,
                .permission = permission,
                .state = memoryState,
                .attributes = memory::MemoryAttribute{
                    .isBorrowed = objectType == KType::KTransferMemory,
                },
            });
        }
    }

    KSharedMemory::~KSharedMemory() {
        if (state.process && guest.Valid()) {
            if (objectType != KType::KTransferMemory) {
                mmap(guest.ptr, guest.size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0); // It doesn't particularly matter if this fails as it shouldn't really affect anything
                state.process->memory.InsertChunk(ChunkDescriptor{
                    .ptr = guest.ptr,
                    .size = guest.size,
                    .state = memory::states::Unmapped,
                });
            } else {
                // KTransferMemory remaps the region with R/W permissions during destruction
                constexpr memory::Permission UnborrowPermission{true, true, false};

                if (mmap(guest.ptr, guest.size, UnborrowPermission.Get(), MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0) == MAP_FAILED)
                    // It is likely that these exceptions will end up as asserts as the destructor is implicitly noexcept but we want to be notified about these not working so that's fine as it can be discovered in the debugger
                    throw exception("An error occurred while remapping transfer memory as anonymous memory in guest: {}", strerror(errno));
                else if (!host.Valid())
                    throw exception("Expected host mapping of transfer memory to be valid during KTransferMemory destruction");

                std::memcpy(guest.ptr, host.ptr, host.size);

                state.process->memory.InsertChunk(ChunkDescriptor{
                    .ptr = guest.ptr,
                    .size = guest.size,
                    .permission = UnborrowPermission,
                    .state = memoryState,
                    .attributes = memory::MemoryAttribute{
                        .isBorrowed = false,
                    }
                });
            }
        }

        if (host.Valid())
            munmap(host.ptr, host.size);

        close(fd);
    }
}
