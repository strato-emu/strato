// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "gmmu.h"

namespace skyline::soc::gmmu {
    constexpr u64 GpuPageSize{1 << 16}; //!< The page size of the GPU address space

    GraphicsMemoryManager::GraphicsMemoryManager(const DeviceState &state) : state(state) {
        constexpr u64 gpuAddressSpaceSize{1UL << 40}; //!< The size of the GPU address space
        constexpr u64 gpuAddressSpaceBase{0x100000}; //!< The base of the GPU address space - must be non-zero

        // Create the initial chunk that will be split to create new chunks
        ChunkDescriptor baseChunk(gpuAddressSpaceBase, gpuAddressSpaceSize, nullptr, ChunkState::Unmapped);
        chunks.push_back(baseChunk);
    }

    std::optional<ChunkDescriptor> GraphicsMemoryManager::FindChunk(ChunkState desiredState, u64 size, u64 alignment) {
        auto chunk{std::find_if(chunks.begin(), chunks.end(), [desiredState, size, alignment](const ChunkDescriptor &chunk) -> bool {
            return (alignment ? util::IsAligned(chunk.virtualAddress, alignment) : true) && chunk.size > size && chunk.state == desiredState;
        })};

        if (chunk != chunks.end())
            return *chunk;

        return std::nullopt;
    }

    u64 GraphicsMemoryManager::InsertChunk(const ChunkDescriptor &newChunk) {
        auto chunkEnd{chunks.end()};
        for (auto chunk{chunks.begin()}; chunk != chunkEnd; chunk++) {
            if (chunk->CanContain(newChunk)) {
                auto oldChunk{*chunk};
                u64 newSize{newChunk.virtualAddress - chunk->virtualAddress};
                u64 extension{chunk->size - newSize - newChunk.size};

                if (newSize == 0) {
                    *chunk = newChunk;
                } else {
                    chunk->size = newSize;
                    chunk = chunks.insert(std::next(chunk), newChunk);
                }

                if (extension)
                    chunks.insert(std::next(chunk), ChunkDescriptor(newChunk.virtualAddress + newChunk.size, extension, (oldChunk.state == ChunkState::Mapped) ? (oldChunk.cpuPtr + newSize + newChunk.size) : nullptr, oldChunk.state));

                return newChunk.virtualAddress;
            } else if (chunk->virtualAddress + chunk->size > newChunk.virtualAddress) {
                chunk->size = newChunk.virtualAddress - chunk->virtualAddress;

                // Deletes all chunks that are within the chunk being inserted and split the final one
                auto tailChunk{std::next(chunk)};
                while (tailChunk != chunkEnd) {
                    if (tailChunk->virtualAddress + tailChunk->size >= newChunk.virtualAddress + newChunk.size)
                        break;

                    tailChunk = chunks.erase(tailChunk);
                    chunkEnd = chunks.end();
                }

                // The given chunk is too large to fit into existing chunks
                if (tailChunk == chunkEnd)
                    break;

                u64 chunkSliceOffset{newChunk.virtualAddress + newChunk.size - tailChunk->virtualAddress};
                tailChunk->virtualAddress += chunkSliceOffset;
                tailChunk->size -= chunkSliceOffset;
                if (tailChunk->state == ChunkState::Mapped)
                    tailChunk->cpuPtr += chunkSliceOffset;

                // If the size of the head chunk is zero then we can directly replace it with our new one rather than inserting it
                auto headChunk{std::prev(tailChunk)};
                if (headChunk->size == 0)
                    *headChunk = newChunk;
                else
                    chunks.insert(std::next(headChunk), newChunk);

                return newChunk.virtualAddress;
            }
        }

        throw exception("Failed to insert chunk into GPU address space!");
    }

    u64 GraphicsMemoryManager::ReserveSpace(u64 size, u64 alignment) {
        size = util::AlignUp(size, GpuPageSize);

        std::unique_lock lock(mutex);
        auto newChunk{FindChunk(ChunkState::Unmapped, size, alignment)};
        if (!newChunk) [[unlikely]]
            return 0;

        auto chunk{*newChunk};
        chunk.size = size;
        chunk.state = ChunkState::Reserved;

        return InsertChunk(chunk);
    }

    u64 GraphicsMemoryManager::ReserveFixed(u64 virtualAddress, u64 size) {
        if (!util::IsAligned(virtualAddress, GpuPageSize)) [[unlikely]]
            return 0;

        size = util::AlignUp(size, GpuPageSize);

        std::unique_lock lock(mutex);
        return InsertChunk(ChunkDescriptor(virtualAddress, size, nullptr, ChunkState::Reserved));
    }

    u64 GraphicsMemoryManager::MapAllocate(u8 *cpuPtr, u64 size) {
        size = util::AlignUp(size, GpuPageSize);

        std::unique_lock lock(mutex);
        auto mappedChunk{FindChunk(ChunkState::Unmapped, size)};
        if (!mappedChunk) [[unlikely]]
            return 0;

        auto chunk{*mappedChunk};
        chunk.cpuPtr = cpuPtr;
        chunk.size = size;
        chunk.state = ChunkState::Mapped;

        return InsertChunk(chunk);
    }

    u64 GraphicsMemoryManager::MapFixed(u64 virtualAddress, u8 *cpuPtr, u64 size) {
        if (!util::IsAligned(virtualAddress, GpuPageSize)) [[unlikely]]
            return 0;

        size = util::AlignUp(size, GpuPageSize);

        std::unique_lock lock(mutex);
        return InsertChunk(ChunkDescriptor(virtualAddress, size, cpuPtr, ChunkState::Mapped));
    }

    bool GraphicsMemoryManager::Unmap(u64 virtualAddress, u64 size) {
        if (!util::IsAligned(virtualAddress, GpuPageSize)) [[unlikely]]
            return false;

        try {
            std::unique_lock lock(mutex);
            InsertChunk(ChunkDescriptor(virtualAddress, size, nullptr, ChunkState::Unmapped));
        } catch (const std::exception &e) {
            return false;
        }

        return true;
    }

    void GraphicsMemoryManager::Read(u8 *destination, u64 virtualAddress, u64 size) {
        std::shared_lock lock(mutex);

        auto chunk{std::upper_bound(chunks.begin(), chunks.end(), virtualAddress, [](const u64 address, const ChunkDescriptor &chunk) -> bool {
            return address < chunk.virtualAddress;
        })};

        if (chunk == chunks.end() || chunk->state != ChunkState::Mapped)
            throw exception("Failed to read region in GPU address space: Address: 0x{:X}, Size: 0x{:X}", virtualAddress, size);

        chunk--;

        u64 initialSize{size};
        u64 chunkOffset{virtualAddress - chunk->virtualAddress};
        u8 *source{chunk->cpuPtr + chunkOffset};
        u64 sourceSize{std::min(chunk->size - chunkOffset, size)};

        // A continuous region in the GPU address space may be made up of several discontinuous regions in physical memory so we have to iterate over all chunks
        while (size) {
            std::memcpy(destination + (initialSize - size), source, sourceSize);

            size -= sourceSize;
            if (size) {
                if (++chunk == chunks.end() || chunk->state != ChunkState::Mapped)
                    throw exception("Failed to read region in GPU address space: Address: 0x{:X}, Size: 0x{:X}", virtualAddress, size);

                source = chunk->cpuPtr;
                sourceSize = std::min(chunk->size, size);
            }
        }
    }

    void GraphicsMemoryManager::Write(u8 *source, u64 virtualAddress, u64 size) {
        std::shared_lock lock(mutex);

        auto chunk{std::upper_bound(chunks.begin(), chunks.end(), virtualAddress, [](const u64 address, const ChunkDescriptor &chunk) -> bool {
            return address < chunk.virtualAddress;
        })};

        if (chunk == chunks.end() || chunk->state != ChunkState::Mapped)
            throw exception("Failed to write region in GPU address space: Address: 0x{:X}, Size: 0x{:X}", virtualAddress, size);

        chunk--;

        u64 initialSize{size};
        u64 chunkOffset{virtualAddress - chunk->virtualAddress};
        u8 *destination{chunk->cpuPtr + chunkOffset};
        u64 destinationSize{std::min(chunk->size - chunkOffset, size)};

        // A continuous region in the GPU address space may be made up of several discontinuous regions in physical memory so we have to iterate over all chunks
        while (size) {
            std::memcpy(destination, source + (initialSize - size), destinationSize);

            size -= destinationSize;
            if (size) {
                if (++chunk == chunks.end() || chunk->state != ChunkState::Mapped)
                    throw exception("Failed to write region in GPU address space: Address: 0x{:X}, Size: 0x{:X}", virtualAddress, size);

                destination = chunk->cpuPtr;
                destinationSize = std::min(chunk->size, size);
            }
        }
    }
}
