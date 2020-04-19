// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <asm/unistd.h>
#include <nce.h>
#include <os.h>
#include "KTransferMemory.h"

namespace skyline::kernel::type {
    KTransferMemory::KTransferMemory(const DeviceState &state, bool host, u64 address, size_t size, const memory::Permission permission, memory::MemoryState memState) : host(host), size(size), KMemory(state, KType::KTransferMemory) {
        if (address && !util::PageAligned(address))
            throw exception("KTransferMemory was created with non-page-aligned address: 0x{:X}", address);

        BlockDescriptor block{
            .size = size,
            .permission = permission,
        };
        ChunkDescriptor chunk{
            .size = size,
            .state = memState,
            .blockList = {block},
        };

        if (host) {
            address = reinterpret_cast<u64>(mmap(reinterpret_cast<void *>(address), size, permission.Get(), MAP_ANONYMOUS | MAP_PRIVATE | ((address) ? MAP_FIXED : 0), -1, 0));
            if (reinterpret_cast<void *>(address) == MAP_FAILED)
                throw exception("An error occurred while mapping transfer memory in host");

            this->address = address;
            chunk.address = address;
            chunk.blockList.front().address = address;
            hostChunk = chunk;
        } else {
            Registers fregs{
                .x0 = address,
                .x1 = size,
                .x2 = static_cast<u64 >(permission.Get()),
                .x3 = static_cast<u64>(MAP_ANONYMOUS | MAP_PRIVATE | ((address) ? MAP_FIXED : 0)),
                .x4 = static_cast<u64>(-1),
                .x8 = __NR_mmap,
            };

            state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
            if (fregs.x0 < 0)
                throw exception("An error occurred while mapping shared region in child process");

            this->address = fregs.x0;
            chunk.address = fregs.x0;
            chunk.blockList.front().address = fregs.x0;

            state.os->memory.InsertChunk(chunk);
        }
    }

    u64 KTransferMemory::Transfer(bool mHost, u64 nAddress, u64 nSize) {
        if (nAddress && !util::PageAligned(nAddress))
            throw exception("KTransferMemory was transferred to a non-page-aligned address: 0x{:X}", nAddress);

        nSize = nSize ? nSize : size;

        ChunkDescriptor chunk = host ? hostChunk : *state.os->memory.GetChunk(address);
        chunk.address = nAddress;
        chunk.size = nSize;
        MemoryManager::ResizeChunk(&chunk, nSize);

        for (auto &block : chunk.blockList) {
            block.address = nAddress + (block.address - address);

            if ((mHost && !host) || (!mHost && !host)) {
                Registers fregs{
                    .x0 = block.address,
                    .x1 = block.size,
                    .x2 = (block.permission.w) ? static_cast<u64>(block.permission.Get()) : (PROT_READ | PROT_WRITE),
                    .x3 = static_cast<u64>(MAP_ANONYMOUS | MAP_PRIVATE | ((nAddress) ? MAP_FIXED : 0)),
                    .x4 = static_cast<u64>(-1),
                    .x8 = __NR_mmap,
                };

                state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
                if (fregs.x0 < 0)
                    throw exception("An error occurred while mapping transfer memory in child process");

                nAddress = fregs.x0;
            } else if ((!mHost && host) || (mHost && host)) {
                nAddress = reinterpret_cast<u64>(mmap(reinterpret_cast<void *>(block.address), block.size, block.permission.Get(), MAP_ANONYMOUS | MAP_PRIVATE | ((nAddress) ? MAP_FIXED : 0), -1, 0));
                if (reinterpret_cast<void *>(nAddress) == MAP_FAILED)
                    throw exception("An error occurred while mapping transfer memory in host");
            }

            if (block.permission.r) {
                if (mHost && !host)
                    state.process->ReadMemory(reinterpret_cast<void *>(nAddress), address, block.size);
                else if (!mHost && host)
                    state.process->WriteMemory(reinterpret_cast<void *>(address), nAddress, block.size);
                else if (!mHost && !host)
                    state.process->CopyMemory(address, nAddress, block.size);
                else if (mHost && host)
                    std::memcpy(reinterpret_cast<void *>(nAddress), reinterpret_cast<void *>(address), block.size);
            }
            if (!block.permission.w) {
                if (mHost) {
                    if (mprotect(reinterpret_cast<void *>(block.address), block.size, block.permission.Get()) == reinterpret_cast<u64>(MAP_FAILED))
                        throw exception("An error occurred while remapping transfer memory: {}", strerror(errno));
                } else {
                    Registers fregs{
                        .x0 = block.address,
                        .x1 = block.size,
                        .x2 = static_cast<u64>(block.permission.Get()),
                        .x8 = __NR_mprotect,
                    };

                    state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
                    if (fregs.x0 < 0)
                        throw exception("An error occurred while updating transfer memory's permissions in guest");
                }
            }
        }

        if (mHost && !host) {
            state.os->memory.DeleteChunk(address);
            hostChunk = chunk;
        } else if (!mHost && host) {
            state.os->memory.InsertChunk(chunk);
        } else if (mHost && host) {
            hostChunk = chunk;
        } else if (!mHost && !host) {
            state.os->memory.DeleteChunk(address);
            state.os->memory.InsertChunk(chunk);
        }

        if ((mHost && !host) || (!mHost && !host)) {
            Registers fregs{
                .x0 = address,
                .x1 = size,
                .x8 = __NR_munmap,
            };

            state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
            if (fregs.x0 < 0)
                throw exception("An error occurred while unmapping transfer memory in child process");
        } else if ((!mHost && host) || (mHost && host)) {
            if (reinterpret_cast<void *>(munmap(reinterpret_cast<void *>(address), size)) == MAP_FAILED)
                throw exception("An error occurred while unmapping transfer memory in host: {}");
        }

        host = mHost;
        address = nAddress;
        size = nSize;
        return address;
    }

    void KTransferMemory::Resize(size_t nSize) {
        if (host) {
            if (mremap(reinterpret_cast<void *>(address), size, nSize, 0) == MAP_FAILED)
                throw exception("An error occurred while remapping transfer memory in host: {}", strerror(errno));
        } else {
            Registers fregs{
                .x0 = address,
                .x1 = size,
                .x2 = nSize,
                .x8 = __NR_mremap,
            };

            state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
            if (fregs.x0 < 0)
                throw exception("An error occurred while remapping transfer memory in guest");

            size = nSize;

            auto chunk = state.os->memory.GetChunk(address);
            MemoryManager::ResizeChunk(chunk, size);
        }
    }

    void KTransferMemory::UpdatePermission(const u64 address, const u64 size, memory::Permission permission) {
        BlockDescriptor block{
            .address = address,
            .size = size,
            .permission = permission,
        };

        if (host) {
            if (mprotect(reinterpret_cast<void *>(address), size, permission.Get()) == reinterpret_cast<u64>(MAP_FAILED))
                throw exception("An occurred while remapping transfer memory: {}", strerror(errno));

            MemoryManager::InsertBlock(&hostChunk, block);
        } else {
            Registers fregs{
                .x0 = address,
                .x1 = size,
                .x2 = static_cast<u64>(permission.Get()),
                .x8 = __NR_mprotect,
            };

            state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
            if (fregs.x0 < 0)
                throw exception("An error occurred while updating transfer memory's permissions in guest");

            auto chunk = state.os->memory.GetChunk(address);
            MemoryManager::InsertBlock(chunk, block);
        }
    }

    KTransferMemory::~KTransferMemory() {
        if (host) {
            munmap(reinterpret_cast<void *>(address), size);
        } else if (state.process) {
            try {
                Registers fregs{
                    .x0 = address,
                    .x1 = size,
                    .x8 = __NR_munmap,
                };

                state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);

                state.os->memory.DeleteChunk(address);
            } catch (const std::exception &) {
            }
        }
    }
};
