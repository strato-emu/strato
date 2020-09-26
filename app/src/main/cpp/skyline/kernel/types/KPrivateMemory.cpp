// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/sharedmem.h>
#include <asm/unistd.h>
#include <unistd.h>
#include <os.h>
#include <nce.h>
#include "KPrivateMemory.h"
#include "KProcess.h"

namespace skyline::kernel::type {
    KPrivateMemory::KPrivateMemory(const DeviceState &state, u64 address, size_t size, memory::Permission permission, memory::MemoryState memState) : size(size), KMemory(state, KType::KPrivateMemory) {
        if (address && !util::PageAligned(address))
            throw exception("KPrivateMemory was created with non-page-aligned address: 0x{:X}", address);

        fd = ASharedMemory_create("KPrivateMemory", size);
        if (fd < 0)
            throw exception("An error occurred while creating shared memory: {}", fd);

        auto host{mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd, 0)};
        if (host == MAP_FAILED)
            throw exception("An occurred while mapping shared memory: {}", strerror(errno));

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
            throw exception("An error occurred while mapping private memory in child process");

        this->address = fregs.x0;

        BlockDescriptor block{
            .address = fregs.x0,
            .size = size,
            .permission = permission,
        };
        ChunkDescriptor chunk{
            .address = fregs.x0,
            .size = size,
            .host = reinterpret_cast<u64>(host),
            .state = memState,
            .blockList = {block},
        };
        state.os->memory.InsertChunk(chunk);
    }

    void KPrivateMemory::Resize(size_t nSize) {
        if (close(fd) < 0)
            throw exception("An error occurred while trying to close shared memory FD: {}", strerror(errno));

        fd = ASharedMemory_create("KPrivateMemory", nSize);
        if (fd < 0)
            throw exception("An error occurred while creating shared memory: {}", fd);

        Registers fregs{
            .x0 = address,
            .x1 = size,
            .x8 = __NR_munmap
        };

        state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
        if (fregs.x0 < 0)
            throw exception("An error occurred while unmapping private memory in child process");

        fregs = {
            .x0 = address,
            .x1 = nSize,
            .x2 = static_cast<u64>(PROT_READ | PROT_WRITE | PROT_EXEC),
            .x3 = static_cast<u64>(MAP_SHARED | MAP_FIXED),
            .x4 = static_cast<u64>(fd),
            .x8 = __NR_mmap,
        };

        state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
        if (fregs.x0 < 0)
            throw exception("An error occurred while remapping private memory in child process");

        auto chunk{state.os->memory.GetChunk(address)};
        state.process->WriteMemory(reinterpret_cast<void *>(chunk->host), address, std::min(nSize, size), true);

        for (const auto &block : chunk->blockList) {
            if ((block.address - chunk->address) < size) {
                fregs = {
                    .x0 = block.address,
                    .x1 = std::min(block.size, (chunk->address + nSize) - block.address),
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

        munmap(reinterpret_cast<void *>(chunk->host), size);

        auto host{mmap(reinterpret_cast<void *>(chunk->host), nSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd, 0)};
        if (host == MAP_FAILED)
            throw exception("An occurred while mapping shared memory: {}", strerror(errno));

        chunk->host = reinterpret_cast<u64>(host);
        MemoryManager::ResizeChunk(chunk, nSize);
        size = nSize;
    }

    void KPrivateMemory::UpdatePermission(u64 address, u64 size, memory::Permission permission) {
        Registers fregs{
            .x0 = address,
            .x1 = size,
            .x2 = static_cast<u64>(permission.Get()),
            .x8 = __NR_mprotect,
        };

        state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
        if (fregs.x0 < 0)
            throw exception("An error occurred while updating private memory's permissions in child process");

        auto chunk{state.os->memory.GetChunk(address)};

        // If a static code region has been mapped as writable it needs to be changed to mutable
        if (chunk->state.value == memory::states::CodeStatic.value && permission.w)
            chunk->state = memory::states::CodeMutable;

        BlockDescriptor block{
            .address = address,
            .size = size,
            .permission = permission,
        };
        MemoryManager::InsertBlock(chunk, block);
    }

    KPrivateMemory::~KPrivateMemory() {
        try {
            if (state.process) {
                Registers fregs{
                    .x0 = address,
                    .x1 = size,
                    .x8 = __NR_munmap,
                };
                state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
            }
        } catch (const std::exception &) {
        }

        auto chunk{state.os->memory.GetChunk(address)};
        if (chunk) {
            munmap(reinterpret_cast<void *>(chunk->host), chunk->size);
            state.os->memory.DeleteChunk(address);
        }
    }
};
