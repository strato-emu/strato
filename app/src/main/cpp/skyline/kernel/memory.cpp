// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "memory.h"
#include "types/KProcess.h"

namespace skyline::kernel {
    MemoryManager::MemoryManager(const DeviceState &state) : state(state) {}

    constexpr size_t RegionAlignment{1ULL << 21}; //!< The minimum alignment of a HOS memory region

    void MemoryManager::InitializeVmm(memory::AddressSpaceType type) {
        switch (type) {
            case memory::AddressSpaceType::AddressSpace32Bit:
            case memory::AddressSpaceType::AddressSpace32BitNoReserved:
                throw exception("32-bit address spaces are not supported");

            case memory::AddressSpaceType::AddressSpace36Bit: {
                addressSpace.address = 0;
                addressSpace.size = 1UL << 36;
                base.size = 0x78000000 + 0x180000000 + 0x78000000 + 0x180000000;
                throw exception("36-bit address spaces are not supported"); // Due to VMM base being forced at 0x800000 and it being used by ART
            }

            case memory::AddressSpaceType::AddressSpace39Bit: {
                addressSpace.address = 0;
                addressSpace.size = 1UL << 39;
                base.size = 0x78000000 + 0x1000000000 + 0x180000000 + 0x80000000 + 0x1000000000; // Code region size is an assumed maximum here
                break;
            }

            default:
                throw exception("VMM initialization with unknown address space");
        }

        std::ifstream mapsFile("/proc/self/maps");
        std::string maps((std::istreambuf_iterator<char>(mapsFile)), std::istreambuf_iterator<char>());
        size_t line{}, start{}, alignedStart{};
        do {
            auto end{util::HexStringToInt<u64>(std::string_view(maps.data() + line, sizeof(u64) * 2))};
            if (end - start > base.size + (alignedStart - start)) { // We don't want to overflow if alignedStart > start
                base.address = alignedStart;
                break;
            }

            start = util::HexStringToInt<u64>(std::string_view(maps.data() + maps.find_first_of('-', line) + 1, sizeof(u64) * 2));
            alignedStart = util::AlignUp(start, RegionAlignment);
            if (alignedStart + base.size > addressSpace.size)
                break;
        } while ((line = maps.find_first_of('\n', line)) != std::string::npos && line++);

        if (!base.address)
            throw exception("Cannot find a suitable carveout for the guest address space");

        mmap(reinterpret_cast<void *>(base.address), base.size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

        chunks = {ChunkDescriptor{
            .ptr = reinterpret_cast<u8 *>(addressSpace.address),
            .size = addressSpace.size,
            .state = memory::states::Unmapped,
        }};
    }

    void MemoryManager::InitializeRegions(u8 *codeStart, u64 size) {
        u64 address{reinterpret_cast<u64>(codeStart)};
        if (!util::IsAligned(address, RegionAlignment))
            throw exception("Non-aligned code region was used to initialize regions: 0x{:X} - 0x{:X}", codeStart, codeStart + size);

        switch (addressSpace.size) {
            case 1UL << 36: {
                code.address = 0x800000;
                code.size = 0x78000000;
                if (code.address > address || (code.size - (address - code.address)) < size)
                    throw exception("Code mapping larger than 36-bit code region");
                alias.address = code.address + code.size;
                alias.size = 0x180000000;
                stack.address = alias.address + alias.size;
                stack.size = 0x78000000;
                tlsIo = stack; //!< TLS/IO is shared with Stack on 36-bit
                heap.address = stack.address + stack.size;
                heap.size = 0x180000000;
                break;
            }

            case 1UL << 39: {
                code.address = base.address;
                code.size = util::AlignUp(size, RegionAlignment);
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

            default:
                throw exception("Regions initialized without VMM initialization");
        }

        auto newSize{code.size + alias.size + stack.size + heap.size + ((addressSpace.size == 1UL << 39) ? tlsIo.size : 0)};
        if (newSize > base.size)
            throw exception("Region size has exceeded pre-allocated area: 0x{:X}/0x{:X}", newSize, base.size);
        if (newSize != base.size)
            munmap(reinterpret_cast<u8 *>(base.address) + base.size, newSize - base.size);

        if (size > code.size)
            throw exception("Code region ({}) is smaller than mapped code size ({})", code.size, size);

        state.logger->Debug("Region Map:\nVMM Base: 0x{:X}\nCode Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nAlias Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nHeap Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nStack Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nTLS/IO Region: 0x{:X} - 0x{:X} (Size: 0x{:X})", base.address, code.address, code.address + code.size, code.size, alias.address, alias.address + alias.size, alias.size, heap.address, heap
            .address + heap.size, heap.size, stack.address, stack.address + stack.size, stack.size, tlsIo.address, tlsIo.address + tlsIo.size, tlsIo.size);
    }

    void MemoryManager::InsertChunk(const ChunkDescriptor &chunk) {
        std::unique_lock lock(mutex);

        auto upper{std::upper_bound(chunks.begin(), chunks.end(), chunk.ptr, [](const u8 *ptr, const ChunkDescriptor &chunk) -> bool { return ptr < chunk.ptr; })};
        if (upper == chunks.begin())
            throw exception("InsertChunk: Chunk inserted outside address space: 0x{:X} - 0x{:X} and 0x{:X} - 0x{:X}", upper->ptr, upper->ptr + upper->size, chunk.ptr, chunk.ptr + chunk.size);

        upper = chunks.erase(upper, std::upper_bound(upper, chunks.end(), chunk.ptr + chunk.size, [](const u8 *ptr, const ChunkDescriptor &chunk) -> bool { return ptr < chunk.ptr + chunk.size; }));
        if (upper != chunks.end() && upper->ptr < chunk.ptr + chunk.size) {
            auto end{upper->ptr + upper->size};
            upper->ptr = chunk.ptr + chunk.size;
            upper->size = end - upper->ptr;
        }

        auto lower{std::prev(upper)};
        if (lower->ptr == chunk.ptr && lower->size == chunk.size) {
            lower->state = chunk.state;
            lower->permission = chunk.permission;
            lower->attributes = chunk.attributes;
        } else if (lower->ptr + lower->size > chunk.ptr + chunk.size) {
            auto lowerExtension{*lower};
            lowerExtension.ptr = chunk.ptr + chunk.size;
            lowerExtension.size = (lower->ptr + lower->size) - lowerExtension.ptr;

            lower->size = chunk.ptr - lower->ptr;
            if (lower->size) {
                upper = chunks.insert(upper, lowerExtension);
                chunks.insert(upper, chunk);
            } else {
                auto lower2{std::prev(lower)};
                if (chunk.IsCompatible(*lower2) && lower2->ptr + lower2->size >= chunk.ptr) {
                    lower2->size = chunk.ptr + chunk.size - lower2->ptr;
                    upper = chunks.erase(lower);
                } else {
                    *lower = chunk;
                }
                upper = chunks.insert(upper, lowerExtension);
            }
        } else if (chunk.IsCompatible(*lower) && lower->ptr + lower->size >= chunk.ptr) {
            lower->size = chunk.ptr + chunk.size - lower->ptr;
        } else {
            if (lower->ptr + lower->size > chunk.ptr)
                lower->size = chunk.ptr - lower->ptr;
            if (upper != chunks.end() && chunk.IsCompatible(*upper) && chunk.ptr + chunk.size >= upper->ptr) {
                upper->ptr = chunk.ptr;
                upper->size = chunk.size + upper->size;
            } else {
                chunks.insert(upper, chunk);
            }
        }
    }

    std::optional<ChunkDescriptor> MemoryManager::Get(void *ptr) {
        std::shared_lock lock(mutex);

        auto chunk{std::upper_bound(chunks.begin(), chunks.end(), reinterpret_cast<u8 *>(ptr), [](const u8 *ptr, const ChunkDescriptor &chunk) -> bool { return ptr < chunk.ptr; })};
        if (chunk-- != chunks.begin())
            if ((chunk->ptr + chunk->size) > ptr)
                return std::make_optional(*chunk);

        return std::nullopt;
    }

    size_t MemoryManager::GetMemoryUsage() {
        std::shared_lock lock(mutex);
        size_t size{};
        for (const auto &chunk : chunks)
            if (chunk.state != memory::states::Unmapped)
                size += chunk.size;
        return size;
    }

    size_t MemoryManager::GetKMemoryBlockSize() {
        std::shared_lock lock(mutex);
        constexpr size_t KMemoryBlockSize{0x40};
        return util::AlignUp(chunks.size() * KMemoryBlockSize, PAGE_SIZE);
    }
}
