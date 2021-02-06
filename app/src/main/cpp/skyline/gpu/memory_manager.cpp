// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "memory_manager.h"

namespace skyline::gpu::vmm {
    MemoryManager::MemoryManager(const DeviceState &state) : state(state) {
        constexpr u64 gpuAddressSpaceSize{1UL << 40}; //!< The size of the GPU address space
        constexpr u64 gpuAddressSpaceBase{0x100000}; //!< The base of the GPU address space - must be non-zero

        // Create the initial chunk that will be split to create new chunks
        ChunkDescriptor baseChunk(gpuAddressSpaceBase, gpuAddressSpaceSize, nullptr, ChunkState::Unmapped);
        chunks.push_back(baseChunk);
    }

    std::optional<ChunkDescriptor> MemoryManager::FindChunk(ChunkState desiredState, u64 size, u64 alignment) {
        auto chunk{std::find_if(chunks.begin(), chunks.end(), [desiredState, size, alignment](const ChunkDescriptor &chunk) -> bool {
            return (alignment ? util::IsAligned(chunk.virtAddr, alignment) : true) && chunk.size > size && chunk.state == desiredState;
        })};

        if (chunk != chunks.end())
            return *chunk;

        return std::nullopt;
    }

    u64 MemoryManager::InsertChunk(const ChunkDescriptor &newChunk) {
        auto chunkEnd{chunks.end()};
        for (auto chunk{chunks.begin()}; chunk != chunkEnd; chunk++) {
            if (chunk->CanContain(newChunk)) {
                auto oldChunk{*chunk};
                u64 newSize{newChunk.virtAddr - chunk->virtAddr};
                u64 extension{chunk->size - newSize - newChunk.size};

                if (newSize == 0) {
                    *chunk = newChunk;
                } else {
                    chunk->size = newSize;
                    chunk = chunks.insert(std::next(chunk), newChunk);
                }

                if (extension)
                    chunks.insert(std::next(chunk), ChunkDescriptor(newChunk.virtAddr + newChunk.size, extension, (oldChunk.state == ChunkState::Mapped) ? (oldChunk.cpuPtr + newSize + newChunk.size) : nullptr, oldChunk.state));

                return newChunk.virtAddr;
            } else if (chunk->virtAddr + chunk->size > newChunk.virtAddr) {
                chunk->size = newChunk.virtAddr - chunk->virtAddr;

                // Deletes all chunks that are within the chunk being inserted and split the final one
                auto tailChunk{std::next(chunk)};
                while (tailChunk != chunkEnd) {
                    if (tailChunk->virtAddr + tailChunk->size >= newChunk.virtAddr + newChunk.size)
                        break;

                    tailChunk = chunks.erase(tailChunk);
                    chunkEnd = chunks.end();
                }

                // The given chunk is too large to fit into existing chunks
                if (tailChunk == chunkEnd)
                    break;

                u64 chunkSliceOffset{newChunk.virtAddr + newChunk.size - tailChunk->virtAddr};
                tailChunk->virtAddr += chunkSliceOffset;
                tailChunk->size -= chunkSliceOffset;
                if (tailChunk->state == ChunkState::Mapped)
                    tailChunk->cpuPtr += chunkSliceOffset;

                // If the size of the head chunk is zero then we can directly replace it with our new one rather than inserting it
                auto headChunk{std::prev(tailChunk)};
                if (headChunk->size == 0)
                    *headChunk = newChunk;
                else
                    chunks.insert(std::next(headChunk), newChunk);

                return newChunk.virtAddr;
            }
        }

        throw exception("Failed to insert chunk into GPU address space!");
    }

    u64 MemoryManager::ReserveSpace(u64 size, u64 alignment) {
        size = util::AlignUp(size, constant::GpuPageSize);

        std::unique_lock lock(vmmMutex);
        auto newChunk{FindChunk(ChunkState::Unmapped, size, alignment)};
        if (!newChunk)
            return 0;

        auto chunk{*newChunk};
        chunk.size = size;
        chunk.state = ChunkState::Reserved;

        return InsertChunk(chunk);
    }

    u64 MemoryManager::ReserveFixed(u64 virtAddr, u64 size) {
        if (!util::IsAligned(virtAddr, constant::GpuPageSize))
            return 0;

        size = util::AlignUp(size, constant::GpuPageSize);

        std::unique_lock lock(vmmMutex);
        return InsertChunk(ChunkDescriptor(virtAddr, size, nullptr, ChunkState::Reserved));
    }

    u64 MemoryManager::MapAllocate(u8 *cpuPtr, u64 size) {
        size = util::AlignUp(size, constant::GpuPageSize);

        std::unique_lock lock(vmmMutex);
        auto mappedChunk{FindChunk(ChunkState::Unmapped, size)};
        if (!mappedChunk)
            return 0;

        auto chunk{*mappedChunk};
        chunk.cpuPtr = cpuPtr;
        chunk.size = size;
        chunk.state = ChunkState::Mapped;

        return InsertChunk(chunk);
    }

    u64 MemoryManager::MapFixed(u64 virtAddr, u8 *cpuPtr, u64 size) {
        if (!util::IsAligned(virtAddr, constant::GpuPageSize))
            return 0;

        size = util::AlignUp(size, constant::GpuPageSize);

        std::unique_lock lock(vmmMutex);
        return InsertChunk(ChunkDescriptor(virtAddr, size, cpuPtr, ChunkState::Mapped));
    }

    bool MemoryManager::Unmap(u64 virtAddr, u64 size) {
        if (!util::IsAligned(virtAddr, constant::GpuPageSize))
            return false;

        try {
            std::unique_lock lock(vmmMutex);
            InsertChunk(ChunkDescriptor(virtAddr, size, nullptr, ChunkState::Unmapped));
        } catch (const std::exception &e) {
            return false;
        }

        return true;
    }

    void MemoryManager::Read(u8 *destination, u64 virtAddr, u64 size) {
        std::shared_lock lock(vmmMutex);

        auto chunk{std::upper_bound(chunks.begin(), chunks.end(), virtAddr, [](const u64 address, const ChunkDescriptor &chunk) -> bool {
            return address < chunk.virtAddr;
        })};

        if (chunk == chunks.end() || chunk->state != ChunkState::Mapped)
            throw exception("Failed to read region in GPU address space: Address: 0x{:X}, Size: 0x{:X}", virtAddr, size);

        chunk--;

        u64 initialSize{size};
        u64 chunkOffset{virtAddr - chunk->virtAddr};
        u8 *source{chunk->cpuPtr + chunkOffset};
        u64 sourceSize{std::min(chunk->size - chunkOffset, size)};

        // A continuous region in the GPU address space may be made up of several discontinuous regions in physical memory so we have to iterate over all chunks
        while (size) {
            std::memcpy(destination + (initialSize - size), source, sourceSize);

            size -= sourceSize;
            if (size) {
                if (++chunk == chunks.end() || chunk->state != ChunkState::Mapped)
                    throw exception("Failed to read region in GPU address space: Address: 0x{:X}, Size: 0x{:X}", virtAddr, size);

                source = chunk->cpuPtr;
                sourceSize = std::min(chunk->size, size);
            }
        }
    }

    void MemoryManager::Write(u8 *source, u64 virtAddr, u64 size) {
        std::shared_lock lock(vmmMutex);

        auto chunk{std::upper_bound(chunks.begin(), chunks.end(), virtAddr, [](const u64 address, const ChunkDescriptor &chunk) -> bool {
            return address < chunk.virtAddr;
        })};

        if (chunk == chunks.end() || chunk->state != ChunkState::Mapped)
            throw exception("Failed to write region in GPU address space: Address: 0x{:X}, Size: 0x{:X}", virtAddr, size);

        chunk--;

        u64 initialSize{size};
        u64 chunkOffset{virtAddr - chunk->virtAddr};
        u8 *destination{chunk->cpuPtr + chunkOffset};
        u64 destinationSize{std::min(chunk->size - chunkOffset, size)};

        // A continuous region in the GPU address space may be made up of several discontinuous regions in physical memory so we have to iterate over all chunks
        while (size) {
            std::memcpy(destination, source + (initialSize - size), destinationSize);

            size -= destinationSize;
            if (size) {
                if (++chunk == chunks.end() || chunk->state != ChunkState::Mapped)
                    throw exception("Failed to write region in GPU address space: Address: 0x{:X}, Size: 0x{:X}", virtAddr, size);

                destination = chunk->cpuPtr;
                destinationSize = std::min(chunk->size, size);
            }
        }
    }
}
