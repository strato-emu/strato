// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "memory.h"
#include "types/KProcess.h"

namespace skyline::kernel {
    ChunkDescriptor *MemoryManager::GetChunk(u64 address) {
        auto chunk{std::upper_bound(chunkList.begin(), chunkList.end(), address, [](const u64 address, const ChunkDescriptor &chunk) -> bool {
            return address < chunk.address;
        })};

        if (chunk-- != chunkList.begin()) {
            if ((chunk->address + chunk->size) > address)
                return chunk.base();
        }

        return nullptr;
    }

    BlockDescriptor *MemoryManager::GetBlock(u64 address, ChunkDescriptor *chunk) {
        if (!chunk)
            chunk = GetChunk(address);

        if (chunk) {
            auto block{std::upper_bound(chunk->blockList.begin(), chunk->blockList.end(), address, [](const u64 address, const BlockDescriptor &block) -> bool {
                return address < block.address;
            })};

            if (block-- != chunk->blockList.begin()) {
                if ((block->address + block->size) > address)
                    return block.base();
            }
        }

        return nullptr;
    }

    void MemoryManager::InsertChunk(const ChunkDescriptor &chunk) {
        auto upperChunk{std::upper_bound(chunkList.begin(), chunkList.end(), chunk.address, [](const u64 address, const ChunkDescriptor &chunk) -> bool {
            return address < chunk.address;
        })};

        if (upperChunk != chunkList.begin()) {
            auto lowerChunk{std::prev(upperChunk)};

            if (lowerChunk->address + lowerChunk->size > chunk.address)
                throw exception("InsertChunk: Descriptors are colliding: 0x{:X} - 0x{:X} and 0x{:X} - 0x{:X}", lowerChunk->address, lowerChunk->address + lowerChunk->size, chunk.address, chunk.address + chunk.size);
        }

        chunkList.insert(upperChunk, chunk);
    }

    void MemoryManager::DeleteChunk(u64 address) {
        for (auto chunk{chunkList.begin()}, end{chunkList.end()}; chunk != end;) {
            if (chunk->address <= address && (chunk->address + chunk->size) > address)
                chunk = chunkList.erase(chunk);
            else
                chunk++;
        }
    }

    void MemoryManager::ResizeChunk(ChunkDescriptor *chunk, size_t size) {
        if (chunk->blockList.size() == 1) {
            chunk->blockList.begin()->size = size;
        } else if (size > chunk->size) {
            auto begin{chunk->blockList.begin()};
            auto end{std::prev(chunk->blockList.end())};

            BlockDescriptor block{
                .address = (end->address + end->size),
                .size = (chunk->address + size) - (end->address + end->size),
                .permission = begin->permission,
                .attributes = begin->attributes,
            };

            chunk->blockList.push_back(block);
        } else if (size < chunk->size) {
            auto endAddress{chunk->address + size};

            for (auto block{chunk->blockList.begin()}, end{chunk->blockList.end()}; block != end;) {
                if (block->address > endAddress)
                    block = chunk->blockList.erase(block);
                else
                    block++;
            }

            auto end{std::prev(chunk->blockList.end())};
            end->size = endAddress - end->address;
        }

        chunk->size = size;
    }

    void MemoryManager::InsertBlock(ChunkDescriptor *chunk, BlockDescriptor block) {
        if (chunk->address + chunk->size < block.address + block.size)
            throw exception("InsertBlock: Inserting block past chunk end is not allowed");

        for (auto iter{chunk->blockList.begin()}; iter != chunk->blockList.end(); iter++) {
            if (iter->address <= block.address) {
                if ((iter->address + iter->size) > block.address) {
                    if (iter->address == block.address && iter->size == block.size) {
                        iter->attributes = block.attributes;
                        iter->permission = block.permission;
                    } else {
                        auto endBlock{*iter};
                        endBlock.address = (block.address + block.size);
                        endBlock.size = (iter->address + iter->size) - endBlock.address;

                        iter->size = block.address - iter->address;
                        chunk->blockList.insert(std::next(iter), {block, endBlock});
                    }
                    return;
                }
            }
        }

        throw exception("InsertBlock: Block offset not present within current block list");
    }

    void MemoryManager::InitializeRegions(u64 address, u64 size, memory::AddressSpaceType type) {
        switch (type) {
            case memory::AddressSpaceType::AddressSpace32Bit:
                throw exception("32-bit address spaces are not supported");

            case memory::AddressSpaceType::AddressSpace36Bit: {
                addressSpace.address = 0;
                addressSpace.size = 1UL << 36;
                base.address = constant::BaseAddress;
                base.size = 0xFF8000000;
                code.address = base.address;
                code.size = 0x78000000;
                if (code.address > address || (code.size - (address - code.address)) < size)
                    throw exception("Code mapping larger than 36-bit code region");
                alias.address = code.address + code.size;
                alias.size = 0x180000000;
                stack.address = alias.address;
                stack.size = alias.size;
                heap.address = alias.address + alias.size;
                heap.size = 0x180000000;
                tlsIo.address = code.address;
                tlsIo.size = 0;
                break;
            }

            case memory::AddressSpaceType::AddressSpace39Bit: {
                addressSpace.address = 0;
                addressSpace.size = 1UL << 39;
                base.address = constant::BaseAddress;
                base.size = 0x7FF8000000;
                code.address = util::AlignDown(address, 0x200000);
                code.size = util::AlignUp(address + size, 0x200000) - code.address;
                alias.address = code.address + code.size;
                alias.size = 0x1000000000;
                heap.address = alias.address + alias.size;
                heap.size = 0x180000000;
                stack.address = heap.address + heap.size;
                stack.size = 0x80000000;
                tlsIo.address = stack.address + stack.size;
                tlsIo.size = 0x1000000000;
                break;
            }
        }

        state.logger->Debug("Region Map:\nCode Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nAlias Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nHeap Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nStack Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nTLS/IO Region: 0x{:X} - 0x{:X} (Size: 0x{:X})", code.address, code.address + code.size, code.size, alias.address, alias.address + alias.size, alias.size, heap.address, heap
            .address + heap.size, heap.size, stack.address, stack.address + stack.size, stack.size, tlsIo.address, tlsIo.address + tlsIo.size, tlsIo.size);
    }

    MemoryManager::MemoryManager(const DeviceState &state) : state(state) {}

    std::optional<DescriptorPack> MemoryManager::Get(u64 address, bool requireMapped) {
        auto chunk{GetChunk(address)};

        if (chunk)
            return DescriptorPack{*GetBlock(address, chunk), *chunk};

        // If the requested address is in the address space but no chunks are present then we return a new unmapped region
        if (addressSpace.IsInside(address) && !requireMapped) {
            auto upperChunk{std::upper_bound(chunkList.begin(), chunkList.end(), address, [](const u64 address, const ChunkDescriptor &chunk) -> bool {
                return address < chunk.address;
            })};

            u64 upperAddress{};
            u64 lowerAddress{};

            if (upperChunk != chunkList.end()) {
                upperAddress = upperChunk->address;

                if (upperChunk == chunkList.begin()) {
                    lowerAddress = addressSpace.address;
                } else {
                    upperChunk--;
                    lowerAddress = upperChunk->address + upperChunk->size;
                }
            } else {
                upperAddress = addressSpace.address + addressSpace.size;
                lowerAddress = chunkList.back().address + chunkList.back().size;
            }

            u64 size{upperAddress - lowerAddress};

            return DescriptorPack{
                .chunk = {
                    .address = lowerAddress,
                    .size = size,
                    .state = memory::states::Unmapped
                },
                .block = {
                    .address = lowerAddress,
                    .size = size,
                }
            };
        }

        return std::nullopt;
    }

    size_t MemoryManager::GetProgramSize() {
        size_t size{};
        for (const auto &chunk : chunkList)
            size += chunk.size;
        return size;
    }
}
