// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "memory_manager.h"

namespace skyline::gpu::vmm {
    MemoryManager::MemoryManager(const DeviceState &state) : state(state) {
        constexpr u64 GpuAddressSpaceSize{1ul << 40}; //!< The size of the GPU address space
        constexpr u64 GpuAddressSpaceBase{0x100000}; //!< The base of the GPU address space - must be non-zero

        // Create the initial chunk that will be split to create new chunks
        ChunkDescriptor baseChunk(GpuAddressSpaceBase, GpuAddressSpaceSize, 0, ChunkState::Unmapped);
        chunks.push_back(baseChunk);
    }

    std::optional<ChunkDescriptor> MemoryManager::FindChunk(u64 size, ChunkState state) {
        auto chunk{std::find_if(chunks.begin(), chunks.end(), [size, state](const ChunkDescriptor &chunk) -> bool {
            return chunk.size > size && chunk.state == state;
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
                u64 newSize{newChunk.address - chunk->address};
                u64 extension{chunk->size - newSize - newChunk.size};

                if (newSize == 0) {
                    *chunk = newChunk;
                } else {
                    chunk->size = newSize;

                    chunk = chunks.insert(std::next(chunk), newChunk);
                }

                if (extension)
                    chunks.insert(std::next(chunk), ChunkDescriptor(newChunk.address + newChunk.size, extension, (oldChunk.state == ChunkState::Mapped) ? (oldChunk.cpuAddress + newSize + newChunk.size) : 0, oldChunk.state));

                return newChunk.address;
            } else if (chunk->address + chunk->size > newChunk.address) {
                chunk->size = newChunk.address - chunk->address;

                // Deletes all chunks that are within the chunk being inserted and split the final one
                auto tailChunk{std::next(chunk)};
                while (tailChunk != chunkEnd) {
                    if (tailChunk->address + tailChunk->size >= newChunk.address + newChunk.size)
                        break;

                    tailChunk = chunks.erase(tailChunk);
                    chunkEnd = chunks.end();
                }

                // The given chunk is too large to fit into existing chunks
                if (tailChunk == chunkEnd)
                    break;

                u64 chunkSliceOffset{newChunk.address + newChunk.size - tailChunk->address};
                tailChunk->address += chunkSliceOffset;
                tailChunk->size -= chunkSliceOffset;
                if (tailChunk->state == ChunkState::Mapped)
                    tailChunk->cpuAddress += chunkSliceOffset;


                // If the size of the head chunk is zero then we can directly replace it with our new one rather than inserting it
                auto headChunk{std::prev(tailChunk)};
                if (headChunk->size == 0)
                    *headChunk = newChunk;
                else
                    chunks.insert(std::next(headChunk), newChunk);

                return newChunk.address;
            }
        }

        throw exception("Failed to insert chunk into GPU address space!");
    }

    u64 MemoryManager::ReserveSpace(u64 size) {
        size = util::AlignUp(size, constant::GpuPageSize);
        auto newChunk{FindChunk(size, ChunkState::Unmapped)};
        if (!newChunk)
            return 0;

        auto chunk{*newChunk};
        chunk.size = size;
        chunk.state = ChunkState::Reserved;

        return InsertChunk(chunk);
    }

    u64 MemoryManager::ReserveFixed(u64 address, u64 size) {
        if (!util::IsAligned(address, constant::GpuPageSize))
            return false;

        size = util::AlignUp(size, constant::GpuPageSize);

        return InsertChunk(ChunkDescriptor(address, size, 0, ChunkState::Reserved));
    }

    u64 MemoryManager::MapAllocate(u64 address, u64 size) {
        size = util::AlignUp(size, constant::GpuPageSize);
        auto mappedChunk{FindChunk(size, ChunkState::Unmapped)};
        if (!mappedChunk)
            return 0;

        auto chunk{*mappedChunk};
        chunk.cpuAddress = address;
        chunk.size = size;
        chunk.state = ChunkState::Mapped;

        return InsertChunk(chunk);
    }

    u64 MemoryManager::MapFixed(u64 address, u64 cpuAddress, u64 size) {
        if (!util::IsAligned(address, constant::GpuPageSize))
            return false;

        size = util::AlignUp(size, constant::GpuPageSize);

        return InsertChunk(ChunkDescriptor(address, size, cpuAddress, ChunkState::Mapped));
    }

    bool MemoryManager::Unmap(u64 address) {
        if (!util::IsAligned(address, constant::GpuPageSize))
            return false;

        auto chunk{std::find_if(chunks.begin(), chunks.end(), [address](const ChunkDescriptor &chunk) -> bool {
            return chunk.address == address;
        })};

        if (chunk == chunks.end())
            return false;

        chunk->state = ChunkState::Reserved;
        chunk->cpuAddress = 0;

        return true;
    }

    void MemoryManager::Read(u8 *destination, u64 address, u64 size) const {
        auto chunk{std::upper_bound(chunks.begin(), chunks.end(), address, [](const u64 address, const ChunkDescriptor &chunk) -> bool {
            return address < chunk.address;
        })};

        if (chunk == chunks.end() || chunk->state != ChunkState::Mapped)
            throw exception("Failed to read region in GPU address space: Address: 0x{:X}, Size: 0x{:X}", address, size);

        chunk--;

        u64 initialSize{size};
        u64 chunkOffset{address - chunk->address};
        u64 readAddress{chunk->cpuAddress + chunkOffset};
        u64 readSize{std::min(chunk->size - chunkOffset, size)};

        // A continuous region in the GPU address space may be made up of several discontinuous regions in physical memory so we have to iterate over all chunks
        while (size) {
            state.process->ReadMemory(destination + (initialSize - size), readAddress, readSize);

            size -= readSize;
            if (size) {
                if (++chunk == chunks.end() || chunk->state != ChunkState::Mapped)
                    throw exception("Failed to read region in GPU address space: Address: 0x{:X}, Size: 0x{:X}", address, size);

                readAddress = chunk->cpuAddress;
                readSize = std::min(chunk->size, size);
            }
        }
    }

    void MemoryManager::Write(u8 *source, u64 address, u64 size) const {
        auto chunk{std::upper_bound(chunks.begin(), chunks.end(), address, [](const u64 address, const ChunkDescriptor &chunk) -> bool {
            return address < chunk.address;
        })};

        if (chunk == chunks.end() || chunk->state != ChunkState::Mapped)
            throw exception("Failed to write region in GPU address space: Address: 0x{:X}, Size: 0x{:X}", address, size);

        chunk--;

        u64 initialSize{size};
        u64 chunkOffset{address - chunk->address};
        u64 writeAddress{chunk->cpuAddress + chunkOffset};
        u64 writeSize{std::min(chunk->size - chunkOffset, size)};

        // A continuous region in the GPU address space may be made up of several discontinuous regions in physical memory so we have to iterate over all chunks
        while (size) {
            state.process->WriteMemory(source + (initialSize - size), writeAddress, writeSize);

            size -= writeSize;
            if (size) {
                if (++chunk == chunks.end() || chunk->state != ChunkState::Mapped)
                    throw exception("Failed to write region in GPU address space: Address: 0x{:X}, Size: 0x{:X}", address, size);

                writeAddress = chunk->cpuAddress;
                writeSize = std::min(chunk->size, size);
            }
        }
    }
}
