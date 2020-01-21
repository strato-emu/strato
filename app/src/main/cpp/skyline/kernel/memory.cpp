#include "memory.h"
#include "types/KProcess.h"

namespace skyline::kernel {
    ChunkDescriptor *MemoryManager::GetChunk(u64 address) {
        for (auto &chunk : chunkList)
            if (chunk.address <= address && (chunk.address + chunk.size) >= address)
                return &chunk;
        return nullptr;
    }

    BlockDescriptor *MemoryManager::GetBlock(u64 address) {
        auto chunk = GetChunk(address);
        if (chunk)
            for (auto &block : chunk->blockList)
                if (block.address <= address && (block.address + block.size) >= address)
                    return &block;
        return nullptr;
    }

    void MemoryManager::InsertChunk(const ChunkDescriptor &chunk) {
        auto it = chunkList.begin();
        if (chunkList.empty() || it->address > chunk.address)
            chunkList.push_front(chunk);
        else {
            auto prevIt = it;
            while (true) {
                if (it == chunkList.end() || (prevIt->address < chunk.address && it->address > chunk.address)) {
                    if (prevIt->address + prevIt->size > chunk.address)
                        throw exception("InsertChunk: Descriptors are colliding: 0x{:X} and 0x{:X}", prevIt->address, chunk.address);
                    chunkList.insert_after(prevIt, chunk);
                    break;
                }
                prevIt = it++;
            }
        }
    }

    void MemoryManager::DeleteChunk(u64 address) {
        chunkList.remove_if([address](const ChunkDescriptor &chunk) {
            return chunk.address <= address && (chunk.address + chunk.size) >= address;
        });
    }

    void MemoryManager::ResizeChunk(ChunkDescriptor *chunk, size_t size) {
        if (std::next(chunk->blockList.begin()) == chunk->blockList.end())
            chunk->blockList.begin()->size = size;
        else if (size > chunk->size) {
            auto end = chunk->blockList.begin();
            for (; std::next(end) != chunk->blockList.end(); end++);
            auto baseBlock = (*chunk->blockList.begin());
            BlockDescriptor block{
                .address = (end->address + end->size),
                .size = (chunk->address + size) - (end->address + end->size),
                .permission = baseBlock.permission,
                .attributes = baseBlock.attributes,
            };
            chunk->blockList.insert_after(end, block);
        } else if (chunk->size < size) {
            auto endAddress = chunk->address + size;
            chunk->blockList.remove_if([endAddress](const BlockDescriptor &block) {
                return block.address > endAddress;
            });
            auto end = chunk->blockList.begin();
            for (; std::next(end) != chunk->blockList.end(); end++);
            end->size = endAddress - end->address;
        }
        chunk->size = size;
    }

    void MemoryManager::InsertBlock(ChunkDescriptor *chunk, BlockDescriptor block) {
        for (auto iter = chunk->blockList.begin(); iter != chunk->blockList.end(); iter++) {
            if (iter->address <= block.address && (iter->address + iter->size) >= block.address) {
                if (iter->address == block.address && iter->size == block.size) {
                    iter->attributes = block.attributes;
                    iter->permission = block.permission;
                } else {
                    auto endBlock = *iter;
                    endBlock.address = (block.address + block.size);
                    endBlock.size = (iter->address + iter->size) - endBlock.address;
                    iter->size = (iter->address - block.address);
                    chunk->blockList.insert_after(iter, {block, endBlock});
                }
                break;
            }
        }
    }

    void MemoryManager::InitializeRegions(u64 address, u64 size, const memory::AddressSpaceType type) {
        switch(type) {
            case memory::AddressSpaceType::AddressSpace32Bit:
                throw exception("32-bit address spaces are not supported");
            case memory::AddressSpaceType::AddressSpace36Bit: {
                code.address = 0x8000000;
                code.size = 0x78000000;
                if(code.address > address || (code.size - (address - code.address)) < size)
                    throw exception("Code mapping larger than 36-bit code region");
                alias.address = code.address + code.size;
                alias.size = 0x180000000;
                stack.address = alias.address;
                stack.size = alias.size;
                heap.address = alias.address + alias.size;
                heap.size = 0x180000000;
                tlsIo.address = heap.address + heap.size;
                tlsIo.size = 0x1000000000;
                break;
            }
            case memory::AddressSpaceType::AddressSpace39Bit: {
                code.address = utils::AlignDown(address, 0x200000);
                code.size = utils::AlignUp(address + size, 0x200000) - code.address;
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

    std::optional<DescriptorPack> MemoryManager::Get(u64 address) {
        auto chunk = GetChunk(address);
        if (chunk)
            for (auto &block : chunk->blockList)
                if (block.address <= address && (block.address + block.size) >= address)
                    return DescriptorPack{block, *chunk};
        return std::nullopt;
    }

    memory::Region MemoryManager::GetRegion(memory::Regions region) {
        switch(region) {
            case memory::Regions::Code:
                return code;
            case memory::Regions::Alias:
                return alias;
            case memory::Regions::Heap:
                return heap;
            case memory::Regions::Stack:
                return stack;
            case memory::Regions::TlsIo:
                return tlsIo;
        }
    }

    size_t MemoryManager::GetProgramSize() {
        size_t size = 0;
        for (const auto &chunk : chunkList)
            size += chunk.size;
        return size;
    }
}
