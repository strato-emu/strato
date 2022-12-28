// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <asm-generic/unistd.h>
#include <fcntl.h>
#include "memory.h"
#include "types/KProcess.h"

namespace skyline::kernel {
    MemoryManager::MemoryManager(const DeviceState &state) : state(state) {}

    MemoryManager::~MemoryManager() {
        if (base.valid() && !base.empty())
            munmap(reinterpret_cast<void *>(base.data()), base.size());
    }

    constexpr size_t RegionAlignment{1ULL << 21}; //!< The minimum alignment of a HOS memory region
    constexpr size_t CodeRegionSize{4ULL * 1024 * 1024 * 1024}; //!< The assumed maximum size of the code region (4GiB)

    static span<u8> AllocateMappedRange(size_t minSize, size_t align, size_t minAddress, size_t maxAddress, bool findLargest) {
        span<u8> region{};
        size_t size{minSize};

        std::ifstream mapsFile("/proc/self/maps");
        std::string maps((std::istreambuf_iterator<char>(mapsFile)), std::istreambuf_iterator<char>());
        size_t line{}, start{minAddress}, alignedStart{minAddress};
        do {
            auto end{util::HexStringToInt<u64>(std::string_view(maps.data() + line, sizeof(u64) * 2))};
            if (end < start)
                continue;
            if (end - start > size + (alignedStart - start)) { // We don't want to overflow if alignedStart > start
                if (findLargest)
                    size = end - start;

                region = span<u8>{reinterpret_cast<u8 *>(alignedStart), size};

                if (!findLargest)
                    break;
            }

            start = util::HexStringToInt<u64>(std::string_view(maps.data() + maps.find_first_of('-', line) + 1, sizeof(u64) * 2));
            alignedStart = util::AlignUp(start, align);
            if (alignedStart + size > maxAddress) // We don't want to map past the end of the address space
                break;
        } while ((line = maps.find_first_of('\n', line)) != std::string::npos && line++);

        if (!region.valid())
            throw exception("Allocation failed");

        auto result{mmap(reinterpret_cast<void *>(region.data()), size, PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED, -1, 0)};
        if (result == MAP_FAILED)
            throw exception("Failed to mmap guest address space: {}", strerror(errno));

        return region;
    }

    void MemoryManager::InitializeVmm(memory::AddressSpaceType type) {
        addressSpaceType = type;

        size_t baseSize{};
        switch (type) {
            case memory::AddressSpaceType::AddressSpace32Bit:
            case memory::AddressSpaceType::AddressSpace32BitNoReserved:
                throw exception("32-bit address spaces are not supported");

            case memory::AddressSpaceType::AddressSpace36Bit: {
                addressSpace = span<u8>{reinterpret_cast<u8 *>(0x8000000), (1ULL << 39) - 0x8000000};
                baseSize = 0x180000000 + 0x78000000 + 0x180000000;
            }

            case memory::AddressSpaceType::AddressSpace39Bit: {
                addressSpace = span<u8>{reinterpret_cast<u8 *>(0), 1ULL << 39};
                baseSize = CodeRegionSize + 0x1000000000 + 0x180000000 + 0x80000000 + 0x1000000000;
                break;
            }

            default:
                throw exception("VMM initialization with unknown address space");
        }

        // Qualcomm KGSL (Kernel Graphic Support Layer/Kernel GPU driver) maps below 35-bits, reserving it causes KGSL to go OOM
        static constexpr size_t KgslReservedRegionSize{1ULL << 35};
        if (type != memory::AddressSpaceType::AddressSpace36Bit) {
            base = AllocateMappedRange(baseSize, RegionAlignment, KgslReservedRegionSize, addressSpace.size(), false);

            chunks = {
                ChunkDescriptor{
                    .ptr = addressSpace.data(),
                    .size = static_cast<size_t>(base.data() - addressSpace.data()),
                    .state = memory::states::Reserved,
                },
                ChunkDescriptor{
                    .ptr = base.data(),
                    .size = base.size(),
                    .state = memory::states::Unmapped,
                },
                ChunkDescriptor{
                    .ptr = base.end().base(),
                    .size = addressSpace.size() - reinterpret_cast<u64>(base.end().base()),
                    .state = memory::states::Reserved,
                }};

            code = base;

        } else {
            base = AllocateMappedRange(baseSize, 1ULL << 36, KgslReservedRegionSize, addressSpace.size(), false);
            codeBase36Bit = AllocateMappedRange(0x32000000, RegionAlignment, 0xC000000, 0x78000000ULL + reinterpret_cast<size_t>(addressSpace.data()), true);

            chunks = {
                ChunkDescriptor{
                    .ptr = addressSpace.data(),
                    .size = static_cast<size_t>(codeBase36Bit.data() - addressSpace.data()),
                    .state = memory::states::Heap,  // We can't use reserved here as rtld uses it to know when to halt memory walking
                },
                ChunkDescriptor{
                    .ptr = codeBase36Bit.data(),
                    .size = codeBase36Bit.size(),
                    .state = memory::states::Unmapped,
                },
                ChunkDescriptor{
                    .ptr = codeBase36Bit.end().base(),
                    .size = static_cast<u64>(base.data() - codeBase36Bit.end().base()),
                    .state = memory::states::Heap,
                },
                ChunkDescriptor{
                    .ptr = base.data(),
                    .size = base.size(),
                    .state = memory::states::Unmapped,
                },
                ChunkDescriptor{
                    .ptr = base.end().base(),
                    .size = addressSpace.size() - reinterpret_cast<u64>(base.end().base()),
                    .state = memory::states::Reserved,
                }};
            code = codeBase36Bit;
        }
    }

    void MemoryManager::InitializeRegions(span<u8> codeRegion) {
        if (!util::IsAligned(codeRegion.data(), RegionAlignment))
            throw exception("Non-aligned code region was used to initialize regions: 0x{:X} - 0x{:X}", codeRegion.data(), codeRegion.end().base());

        switch (addressSpaceType) {
            case memory::AddressSpaceType::AddressSpace36Bit: {
                // Place code, stack and TLS/IO in the lower 36-bits of the host AS and heap past that
                code = span<u8>{codeBase36Bit.data(), util::AlignUp(codeRegion.size(), RegionAlignment)};
                stack = span<u8>{code.end().base(), codeBase36Bit.size() - code.size()};
                tlsIo = stack; //!< TLS/IO is shared with Stack on 36-bit
                alias = span<u8>{base.data(), 0x180000000};
                heap = span<u8>{alias.end().base(), 0x180000000};
                break;
            }

            case memory::AddressSpaceType::AddressSpace39Bit: {
                code = span<u8>{base.data(), util::AlignUp(codeRegion.size(), RegionAlignment)};
                alias = span<u8>{code.end().base(), 0x1000000000};
                heap = span<u8>{alias.end().base(), 0x180000000};
                stack = span<u8>{heap.end().base(), 0x80000000};
                tlsIo = span<u8>{stack.end().base(), 0x1000000000};
                break;
            }

            default:
                throw exception("Regions initialized without VMM initialization");
        }

        auto newSize{code.size() + alias.size() + stack.size() + heap.size() + ((addressSpaceType == memory::AddressSpaceType::AddressSpace39Bit) ? tlsIo.size() : 0)};
        if (newSize > base.size())
            throw exception("Guest VMM size has exceeded host carveout size: 0x{:X}/0x{:X} (Code: 0x{:X}/0x{:X})", newSize, base.size(), code.size(), CodeRegionSize);
        if (newSize != base.size())
            munmap(base.end().base(), newSize - base.size());

        if (codeRegion.size() > code.size())
            throw exception("Code region ({}) is smaller than mapped code size ({})", code.size(), codeRegion.size());

        Logger::Debug("Region Map:\nVMM Base: 0x{:X}\nCode Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nAlias Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nHeap Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nStack Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nTLS/IO Region: 0x{:X} - 0x{:X} (Size: 0x{:X})", base.data(), code.data(), code.end().base(), code.size(), alias.data(), alias.end().base(), alias.size(), heap.data(), heap.end().base(), heap.size(), stack.data(), stack.end().base(), stack.size(), tlsIo.data(), tlsIo.end().base(), tlsIo.size());
    }

    span<u8> MemoryManager::CreateMirror(span<u8> mapping) {
        if (!base.contains(mapping))
            throw exception("Mapping is outside of VMM base: 0x{:X} - 0x{:X}", mapping.data(), mapping.end().base());

        auto offset{static_cast<size_t>(mapping.data() - base.data())};
        if (!util::IsPageAligned(offset) || !util::IsPageAligned(mapping.size()))
            throw exception("Mapping is not aligned to a page: 0x{:X}-0x{:X} (0x{:X})", mapping.data(), mapping.end().base(), offset);

        auto mirror{mremap(mapping.data(), 0, mapping.size(), MREMAP_MAYMOVE)};
        if (mirror == MAP_FAILED)
            throw exception("Failed to create mirror mapping at 0x{:X}-0x{:X} (0x{:X}): {}", mapping.data(), mapping.end().base(), offset, strerror(errno));

        mprotect(mirror, mapping.size(), PROT_READ | PROT_WRITE | PROT_EXEC);

        return span<u8>{reinterpret_cast<u8 *>(mirror), mapping.size()};
    }

    span<u8> MemoryManager::CreateMirrors(const std::vector<span<u8>> &regions) {
        size_t totalSize{};
        for (const auto &region : regions)
            totalSize += region.size();

        auto mirrorBase{mmap(nullptr, totalSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)}; // Reserve address space for all mirrors
        if (mirrorBase == MAP_FAILED)
            throw exception("Failed to create mirror base: {} (0x{:X} bytes)", strerror(errno), totalSize);

        size_t mirrorOffset{};
        for (const auto &region : regions) {
            if (!base.contains(region))
                throw exception("Mapping is outside of VMM base: 0x{:X} - 0x{:X}", region.data(), region.end().base());

            auto offset{static_cast<size_t>(region.data() - base.data())};
            if (!util::IsPageAligned(offset) || !util::IsPageAligned(region.size()))
                throw exception("Mapping is not aligned to a page: 0x{:X}-0x{:X} (0x{:X})", region.data(), region.end().base(), offset);

            auto mirror{mremap(region.data(), 0, region.size(), MREMAP_FIXED | MREMAP_MAYMOVE, reinterpret_cast<u8 *>(mirrorBase) + mirrorOffset)};
            if (mirror == MAP_FAILED)
                throw exception("Failed to create mirror mapping at 0x{:X}-0x{:X} (0x{:X}): {}", region.data(), region.end().base(), offset, strerror(errno));

            mprotect(mirror, region.size(), PROT_READ | PROT_WRITE | PROT_EXEC);

            mirrorOffset += region.size();
        }

        if (mirrorOffset != totalSize)
            throw exception("Mirror size mismatch: 0x{:X} != 0x{:X}", mirrorOffset, totalSize);

        return span<u8>{reinterpret_cast<u8 *>(mirrorBase), totalSize};
    }

    void MemoryManager::FreeMemory(span<u8> memory) {
        u8 *alignedStart{util::AlignUp(memory.data(), constant::PageSize)};
        u8 *alignedEnd{util::AlignDown(memory.end().base(), constant::PageSize)};

        if (alignedStart < alignedEnd)
            if (madvise(alignedStart, static_cast<size_t>(alignedEnd - alignedStart), MADV_REMOVE) == -1)
                throw exception("Failed to free memory: {}", strerror(errno))   ;
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
            upper->size = static_cast<size_t>(end - upper->ptr);
        }

        auto lower{std::prev(upper)};
        if (lower->ptr == chunk.ptr && lower->size == chunk.size) {
            lower->state = chunk.state;
            lower->permission = chunk.permission;
            lower->attributes = chunk.attributes;
            lower->memory = chunk.memory;
        } else if (lower->ptr + lower->size > chunk.ptr + chunk.size) {
            auto lowerExtension{*lower};
            lowerExtension.ptr = chunk.ptr + chunk.size;
            lowerExtension.size = static_cast<size_t>((lower->ptr + lower->size) - lowerExtension.ptr);

            lower->size = static_cast<size_t>(chunk.ptr - lower->ptr);
            if (lower->size) {
                upper = chunks.insert(upper, lowerExtension);
                chunks.insert(upper, chunk);
            } else {
                auto lower2{std::prev(lower)};
                if (chunk.IsCompatible(*lower2) && lower2->ptr + lower2->size >= chunk.ptr) {
                    lower2->size = static_cast<size_t>(chunk.ptr + chunk.size - lower2->ptr);
                    upper = chunks.erase(lower);
                } else {
                    *lower = chunk;
                }
                upper = chunks.insert(upper, lowerExtension);
            }
        } else if (chunk.IsCompatible(*lower) && lower->ptr + lower->size >= chunk.ptr) {
            lower->size = static_cast<size_t>(chunk.ptr + chunk.size - lower->ptr);
        } else {
            if (lower->ptr + lower->size > chunk.ptr)
                lower->size = static_cast<size_t>(chunk.ptr - lower->ptr);
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

    size_t MemoryManager::GetUserMemoryUsage() {
        std::shared_lock lock(mutex);
        size_t size{};
        for (const auto &chunk : chunks)
            if (chunk.state == memory::states::Heap)
                size += chunk.size;
        return size + code.size() + state.process->mainThreadStack->guest.size();
    }

    size_t MemoryManager::GetSystemResourceUsage() {
        std::shared_lock lock(mutex);
        constexpr size_t KMemoryBlockSize{0x40};
        return std::min(static_cast<size_t>(state.process->npdm.meta.systemResourceSize), util::AlignUp(chunks.size() * KMemoryBlockSize, constant::PageSize));
    }
}
