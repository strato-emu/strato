// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "memory_manager.h"

namespace skyline::gpu::vmm {
    MemoryManager::MemoryManager() {
        constexpr u64 GpuAddressSpaceSize = 1ul << 40; //!< The size of the GPU address space
        constexpr u64 GpuAddressSpaceBase = 0x100000; //!< The base of the GPU address space - must be non-zero

        // Create the initial chunk that will be split to create new chunks
        ChunkDescriptor baseChunk(GpuAddressSpaceBase, GpuAddressSpaceSize, 0, ChunkState::Unmapped);
        chunkList.push_back(baseChunk);
    }

    std::optional<ChunkDescriptor> MemoryManager::FindChunk(u64 size, ChunkState state) {
        auto chunk = std::find_if(chunkList.begin(), chunkList.end(), [size, state] (const ChunkDescriptor& chunk) -> bool {
            return chunk.size > size && chunk.state == state;
        });

        if (chunk != chunkList.end())
            return *chunk;

        return std::nullopt;
    }

    u64 MemoryManager::InsertChunk(const ChunkDescriptor &newChunk) {
        auto chunkEnd = chunkList.end();
        for (auto chunk = chunkList.begin(); chunk != chunkEnd; chunk++) {
            if (chunk->CanContain(newChunk)) {
                auto oldChunk = *chunk;
                u64 newSize = newChunk.address - chunk->address;
                u64 extension = chunk->size - newSize - newChunk.size;

                if (newSize == 0) {
                    *chunk = newChunk;
                } else {
                    chunk->size = newSize;

                    chunk = chunkList.insert(std::next(chunk), newChunk);
                }

                if (extension)
                    chunkList.insert(std::next(chunk), ChunkDescriptor(newChunk.address + newChunk.size, extension, oldChunk.cpuAddress + oldChunk.size + newChunk.size, oldChunk.state));

                return newChunk.address;
            } else if (chunk->address + chunk->size >= newChunk.address) {
                chunk->size = (newChunk.address - chunk->address);

                // Deletes all chunks that are within the chunk being inserted and split the final one
                auto tailChunk = std::next(chunk);
                while (tailChunk != chunkEnd) {
                    if (tailChunk->address + tailChunk->size >= newChunk.address + newChunk.size)
                        break;

                    tailChunk = chunkList.erase(tailChunk);
                    chunkEnd = chunkList.end();
                }

                // The given chunk is too large to fit into existing chunks
                if (tailChunk == chunkEnd)
                    break;

                u64 chunkSliceOffset = newChunk.address + newChunk.size - tailChunk->address;
                tailChunk->address += chunkSliceOffset;
                tailChunk->size -= chunkSliceOffset;
                if (tailChunk->state == ChunkState::Mapped)
                    tailChunk->cpuAddress += chunkSliceOffset;

                return newChunk.address;
            }
        }

        throw exception("Failed to insert chunk into GPU address space!");
    }

    u64 MemoryManager::AllocateSpace(u64 size) {
        size = util::AlignUp(size, constant::GpuPageSize);
        auto newChunk = FindChunk(size, ChunkState::Unmapped);
        if (!newChunk.has_value())
            return 0;

        auto chunk = newChunk.value();
        chunk.size = size;
        chunk.state = ChunkState::Allocated;

        return InsertChunk(chunk);
    }

    u64 MemoryManager::AllocateFixed(u64 address, u64 size) {
        if ((address & (constant::GpuPageSize - 1)) != 0)
            return 0;

        size = util::AlignUp(size, constant::GpuPageSize);

        return InsertChunk(ChunkDescriptor(address, size, 0, ChunkState::Allocated));
    }

    u64 MemoryManager::MapAllocated(u64 address, u64 size) {
        size = util::AlignUp(size, constant::GpuPageSize);
        auto mappedChunk = FindChunk(size, ChunkState::Allocated);
        if (!mappedChunk.has_value())
            return 0;

        auto chunk = mappedChunk.value();
        chunk.cpuAddress = address;
        chunk.size = size;
        chunk.state = ChunkState::Mapped;

        return InsertChunk(chunk);
    }

    u64 MemoryManager::MapFixed(u64 address, u64 cpuAddress, u64 size) {
        if ((address & (constant::GpuPageSize - 1)) != 0)
            return 0;

        size = util::AlignUp(size, constant::GpuPageSize);


        return InsertChunk(ChunkDescriptor(address, size, cpuAddress, ChunkState::Mapped));
    }
}
