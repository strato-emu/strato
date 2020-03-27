// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/sharedmem.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <os.h>
#include "KSharedMemory.h"
#include "KProcess.h"

namespace skyline::kernel::type {
    KSharedMemory::KSharedMemory(const DeviceState &state, u64 address, size_t size, const memory::Permission permission, memory::MemoryState memState, int mmapFlags) : initialState(memState), KMemory(state, KType::KSharedMemory) {
        if (address && !utils::PageAligned(address))
            throw exception("KSharedMemory was created with non-page-aligned address: 0x{:X}", address);

        fd = ASharedMemory_create("KSharedMemory", size);
        if (fd < 0)
            throw exception("An error occurred while creating shared memory: {}", fd);

        address = reinterpret_cast<u64>(mmap(reinterpret_cast<void *>(address), size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED | ((address) ? MAP_FIXED : 0) | mmapFlags, fd, 0));
        if (address == reinterpret_cast<u64>(MAP_FAILED))
            throw exception("An occurred while mapping shared memory: {}", strerror(errno));

        kernel = {.address = address, .size = size, .permission = permission};
    }

    u64 KSharedMemory::Map(const u64 address, const u64 size, memory::Permission permission) {
        if (address && !utils::PageAligned(address))
            throw exception("KSharedMemory was mapped to a non-page-aligned address: 0x{:X}", address);

        Registers fregs{
            .x0 = address,
            .x1 = size,
            .x2 = static_cast<u64>(permission.Get()),
            .x3 = static_cast<u64>(MAP_SHARED | ((address) ? MAP_FIXED : 0)),
            .x4 = static_cast<u64>(fd),
            .x8 = __NR_mmap,
        };

        state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
        if (fregs.x0 < 0)
            throw exception("An error occurred while mapping shared memory in guest");

        guest = {.address = fregs.x0, .size = size, .permission = permission};

        BlockDescriptor block{
            .address = fregs.x0,
            .size = size,
            .permission = permission,
        };
        ChunkDescriptor chunk{
            .address = fregs.x0,
            .host = kernel.address,
            .size = size,
            .state = initialState,
            .blockList = {block},
        };
        state.os->memory.InsertChunk(chunk);

        return fregs.x0;
    }

    void KSharedMemory::Resize(size_t size) {
        if (guest.Valid() && kernel.Valid()) {
            if (close(fd) < 0)
                throw exception("An error occurred while trying to close shared memory FD: {}", strerror(errno));

            fd = ASharedMemory_create("KSharedMemory", size);
            if (fd < 0)
                throw exception("An error occurred while creating shared memory: {}", fd);

            Registers fregs{
                .x0 = guest.address,
                .x1 = guest.size,
                .x8 = __NR_munmap
            };

            state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
            if (fregs.x0 < 0)
                throw exception("An error occurred while unmapping private memory in child process");

            fregs = {
                .x0 = guest.address,
                .x1 = size,
                .x2 = static_cast<u64>(PROT_READ | PROT_WRITE | PROT_EXEC),
                .x3 = static_cast<u64>(MAP_SHARED | MAP_FIXED),
                .x4 = static_cast<u64>(fd),
                .x8 = __NR_mmap,
            };

            state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
            if (fregs.x0 < 0)
                throw exception("An error occurred while remapping private memory in child process");

            state.process->WriteMemory(reinterpret_cast<void *>(kernel.address), guest.address, std::min(guest.size, size), true);

            auto chunk = state.os->memory.GetChunk(guest.address);
            for (const auto &block : chunk->blockList) {
                if ((block.address - chunk->address) < guest.size) {
                    fregs = {
                        .x0 = block.address,
                        .x1 = std::min(block.size, (chunk->address + size) - block.address),
                        .x2 = static_cast<u64>(block.permission.Get()),
                        .x8 = __NR_mprotect,
                    };

                    state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
                    if (fregs.x0 < 0)
                        throw exception("An error occurred while updating private memory's permissions in child process");
                } else {
                    break;
                }
            }

            munmap(reinterpret_cast<void *>(kernel.address), kernel.size);

            auto host = mmap(reinterpret_cast<void *>(chunk->host), size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd, 0);
            if (host == MAP_FAILED)
                throw exception("An occurred while mapping shared memory: {}", strerror(errno));

            guest.size = size;
            MemoryManager::ResizeChunk(chunk, size);
        } else if (kernel.Valid()) {
            if (close(fd) < 0)
                throw exception("An error occurred while trying to close shared memory FD: {}", strerror(errno));

            fd = ASharedMemory_create("KSharedMemory", size);
            if (fd < 0)
                throw exception("An error occurred while creating shared memory: {}", fd);

            std::vector<u8> data(std::min(size, kernel.size));
            memcpy(data.data(), reinterpret_cast<const void *>(kernel.address), std::min(size, kernel.size));

            munmap(reinterpret_cast<void *>(kernel.address), kernel.size);

            auto address = mmap(reinterpret_cast<void *>(kernel.address), size, kernel.permission.Get(), MAP_SHARED, fd, 0);
            if (address == MAP_FAILED)
                throw exception("An occurred while mapping shared memory: {}", strerror(errno));

            memcpy(address, data.data(), std::min(size, kernel.size));

            kernel.address = reinterpret_cast<u64>(address);
            kernel.size = size;
        } else {
            throw exception("Cannot resize KSharedMemory that's only on guest");
        }
    }

    void KSharedMemory::UpdatePermission(u64 address, u64 size, memory::Permission permission, bool host) {
        if (guest.Valid() && !host) {
            Registers fregs{
                .x0 = address,
                .x1 = size,
                .x2 = static_cast<u64>(permission.Get()),
                .x8 = __NR_mprotect,
            };

            state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
            if (fregs.x0 < 0)
                throw exception("An error occurred while updating shared memory's permissions in guest");

            auto chunk = state.os->memory.GetChunk(address);
            BlockDescriptor block{
                .address = address,
                .size = size,
                .permission = permission,
            };
            MemoryManager::InsertBlock(chunk, block);
        }
        if (kernel.Valid() && host) {
            if (mprotect(reinterpret_cast<void *>(kernel.address), kernel.size, permission.Get()) == reinterpret_cast<u64>(MAP_FAILED))
                throw exception("An error occurred while remapping shared memory: {}", strerror(errno));
            kernel.permission = permission;
        }
    }

    KSharedMemory::~KSharedMemory() {
        try {
            if (guest.Valid() && state.process) {
                Registers fregs{
                    .x0 = guest.address,
                    .x1 = guest.size,
                    .x8 = __NR_munmap,
                };

                state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
            }
        } catch (const std::exception &) {
        }
        if (kernel.Valid())
            munmap(reinterpret_cast<void *>(kernel.address), kernel.size);
        state.os->memory.DeleteChunk(guest.address);
        close(fd);
    }
};
