// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <asm-generic/unistd.h>
#include <fcntl.h>
#include "memory.h"
#include "types/KProcess.h"

namespace skyline::kernel {
    MemoryManager::MemoryManager(const DeviceState &state) noexcept : state(state), setHeapSize(), chunks() {}

    MemoryManager::~MemoryManager() noexcept {
        if (base.valid() && !base.empty())
            munmap(reinterpret_cast<void *>(base.data()), base.size());
    }

    std::map<u8 *,ChunkDescriptor>::iterator MemoryManager::upper_bound(u8 *address) {
        std::map<u8 *,ChunkDescriptor>::iterator result{chunks.begin()};

        if (chunks.size() != 1) [[likely]]
            while (result->first <= address) {
                ++result;
                if (result->first + result->second.size == addressSpace.end().base())
                    break;
            }

        return result;
    }

    void MemoryManager::MapInternal(std::pair<u8 *, ChunkDescriptor> *newDesc) {
        // The chunk that contains / precedes the new chunk base address
        auto firstChunkBase{upper_bound(newDesc->first)};
        while (newDesc->first <= firstChunkBase->first)
            --firstChunkBase;

        // The chunk that contains / follows the end address of the new chunk
        auto lastChunkBase{upper_bound(newDesc->first + newDesc->second.size)};
        while ((newDesc->first + newDesc->second.size) < lastChunkBase->first)
            --lastChunkBase;

        ChunkDescriptor firstChunk{firstChunkBase->second};
        ChunkDescriptor lastChunk{lastChunkBase->second};

        bool needsReprotection{false};
        bool isUnmapping{newDesc->second.state == memory::states::Unmapped};

        // We cut a hole in a single chunk
        if (firstChunkBase->first == lastChunkBase->first) {
            if (firstChunk.IsCompatible(newDesc->second)) [[unlikely]]
                // No editing necessary
                return;

            if ((firstChunk.state == memory::states::Unmapped) != isUnmapping)
                needsReprotection = true;

            // We edit the chunk's first half
            firstChunk.size = static_cast<size_t>(newDesc->first - firstChunkBase->first);
            chunks[firstChunkBase->first] = firstChunk;

            // We create the chunk's second half
            lastChunk.size = static_cast<size_t>((lastChunkBase->first + lastChunk.size) - (newDesc->first + newDesc->second.size));
            chunks[newDesc->first + newDesc->second.size] = lastChunk;

            // Insert new chunk in between
            chunks[newDesc->first] = newDesc->second;
        } else {
            // If there are descriptors between first and last chunk, delete them
            if ((firstChunkBase->first + firstChunk.size) != lastChunkBase->first) {
                auto tempChunkBase{firstChunkBase};

                ++tempChunkBase;
                while (tempChunkBase->first != lastChunkBase->first) {
                    auto tmp{tempChunkBase++};
                    if ((tmp->second.state == memory::states::Unmapped) != isUnmapping)
                        needsReprotection = true;
                    chunks.erase(tmp);
                }
            }

            bool shouldInsert{true};

            if (firstChunk.IsCompatible(newDesc->second)) {
                shouldInsert = false;

                firstChunk.size = static_cast<size_t>((newDesc->first + newDesc->second.size) - firstChunkBase->first);
                chunks[firstChunkBase->first] = firstChunk;
            } else if ((firstChunkBase->first + firstChunk.size) != newDesc->first) {
                firstChunk.size = static_cast<size_t>(newDesc->first - firstChunkBase->first);

                chunks[firstChunkBase->first] = firstChunk;

                if ((firstChunk.state == memory::states::Unmapped) != isUnmapping)
                    needsReprotection = true;
            }

            if (lastChunk.IsCompatible(newDesc->second)) {
                u8 *oldBase{lastChunkBase->first};
                chunks.erase(lastChunkBase);

                if (shouldInsert) {
                    shouldInsert = false;

                    lastChunk.size = static_cast<size_t>((lastChunk.size + oldBase) - (newDesc->first));

                    chunks[newDesc->first] = lastChunk;
                } else {
                    firstChunk.size = static_cast<size_t>((lastChunk.size + oldBase) - firstChunkBase->first);
                    chunks[firstChunkBase->first] = firstChunk;
                }
            } else if ((newDesc->first + newDesc->second.size) != lastChunkBase->first) {
                lastChunk.size = static_cast<size_t>((lastChunk.size + lastChunkBase->first) - (newDesc->first + newDesc->second.size));

                chunks.erase(lastChunkBase);
                chunks[newDesc->first + newDesc->second.size] = lastChunk;

                if ((lastChunk.state == memory::states::Unmapped) != isUnmapping)
                    needsReprotection = true;
            }

            // Insert if not merged
            if (shouldInsert)
                chunks[newDesc->first] = newDesc->second;
        }

        if (needsReprotection)
            if (mprotect(newDesc->first, newDesc->second.size, !isUnmapping ? PROT_READ | PROT_WRITE | PROT_EXEC : PROT_NONE)) [[unlikely]]
                Logger::Warn("Reprotection failed: {}", strerror(errno));
    }

    void MemoryManager::ForeachChunkinRange(span<skyline::u8> memory, auto editCallback) {
        auto chunkBase{upper_bound(memory.data())};
        if (memory.data() < chunkBase->first)
            --chunkBase;

        ChunkDescriptor resultChunk{chunkBase->second};

        size_t sizeLeft{memory.size()};

        if (chunkBase->first < memory.data()) {
            size_t copySize{std::min<size_t>(resultChunk.size - (static_cast<size_t>(memory.data() - chunkBase->first)), memory.size())};

            std::pair<u8 *, ChunkDescriptor> temp(memory.data(), resultChunk);
            temp.second.size = copySize;
            editCallback(temp);

            ++chunkBase;
            resultChunk = chunkBase->second;
            sizeLeft -= copySize;
        }

        while (sizeLeft) {
            if (sizeLeft < resultChunk.size) {
                std::pair<u8 *, ChunkDescriptor> temp(chunkBase->first, resultChunk);
                temp.second.size = sizeLeft;
                editCallback(temp);
                break;
            } else [[likely]] {
                std::pair<u8 *, ChunkDescriptor> temp(chunkBase->first, resultChunk);

                editCallback(temp);

                sizeLeft = sizeLeft - resultChunk.size;
                ++chunkBase;
                resultChunk = chunkBase->second;
            }
        }
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

        if (!region.valid()) [[unlikely]]
            throw exception("Allocation failed");

        auto result{mmap(reinterpret_cast<void *>(region.data()), size, PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED, -1, 0)};
        if (result == MAP_FAILED) [[unlikely]]
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
                addressSpace = span<u8>{reinterpret_cast<u8 *>(0), (1ULL << 36)};
                baseSize = 0x180000000 + 0x180000000;
                break;
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

            chunks[addressSpace.data()] = ChunkDescriptor{
                .size = addressSpace.size(),
                .state = memory::states::Unmapped,
            };

            code = base;

        } else {
            codeBase36Bit = AllocateMappedRange(0x78000000, RegionAlignment, 0x8000000, KgslReservedRegionSize, false);
            base = AllocateMappedRange(baseSize, RegionAlignment, KgslReservedRegionSize, addressSpace.size(), false);

            if ((reinterpret_cast<u64>(base.data()) + baseSize) > (1ULL << 36)) {
                Logger::Warn("Couldn't fit regions into AS! Resizing AS instead!");
                addressSpace = span<u8>{reinterpret_cast<u8 *>(0), 1ULL << 39};
            }

            chunks[addressSpace.data()] = ChunkDescriptor{
                .size = addressSpace.size(),
                .state = memory::states::Unmapped,
            };

            code = codeBase36Bit;
        }
    }

    void MemoryManager::InitializeRegions(span<u8> codeRegion) {
        if (!util::IsAligned(codeRegion.data(), RegionAlignment)) [[unlikely]]
            throw exception("Non-aligned code region was used to initialize regions: 0x{:X} - 0x{:X}", codeRegion.data(), codeRegion.end().base());

        switch (addressSpaceType) {
            case memory::AddressSpaceType::AddressSpace36Bit: {

                // As a workaround if we can't place the code region at the base of the AS we mark it as inaccessible heap so rtld doesn't crash
                if (codeBase36Bit.data() != reinterpret_cast<u8 *>(0x8000000)) {
                    std::pair<u8 *, ChunkDescriptor> tmp(reinterpret_cast<u8 *>(0x8000000), ChunkDescriptor{
                        .size = reinterpret_cast<size_t>(codeBase36Bit.data() - 0x8000000),
                        .state = memory::states::Heap,
                    });
                    MapInternal(&tmp);
                }

                // Place code, stack and TLS/IO in the lower 36-bits of the host AS and heap and alias past that
                code = span<u8>{codeBase36Bit.data(), codeBase36Bit.data() + 0x70000000};
                stack = span<u8>{codeBase36Bit.data(), codeBase36Bit.data() + 0x78000000};
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

                u64 newSize{code.size() + alias.size() + stack.size() + heap.size() + tlsIo.size()};

                if (newSize > base.size()) [[unlikely]]
                    throw exception("Guest VMM size has exceeded host carveout size: 0x{:X}/0x{:X} (Code: 0x{:X}/0x{:X})", newSize, base.size(), code.size(), CodeRegionSize);

                if (newSize != base.size())
                    munmap(base.end().base(), newSize - base.size());

                break;
            }

            default:
                throw exception("Regions initialized without VMM initialization");
        }

        if (codeRegion.size() > code.size()) [[unlikely]]
            throw exception("Code region ({}) is smaller than mapped code size ({})", code.size(), codeRegion.size());

        Logger::Debug("Region Map:\nVMM Base: 0x{:X}\nCode Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nAlias Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nHeap Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nStack Region: 0x{:X} - 0x{:X} (Size: 0x{:X})\nTLS/IO Region: 0x{:X} - 0x{:X} (Size: 0x{:X})", code.data(), code.data(), code.end().base(), code.size(), alias.data(), alias.end().base(), alias.size(), heap.data(), heap.end().base(), heap.size(), stack.data(), stack.end().base(), stack.size(), tlsIo.data(), tlsIo.end().base(), tlsIo.size());
    }

    span<u8> MemoryManager::CreateMirror(span<u8> mapping) {
        if (!base.contains(mapping)) [[unlikely]]
            throw exception("Mapping is outside of VMM base: 0x{:X} - 0x{:X}", mapping.data(), mapping.end().base());

        auto offset{static_cast<size_t>(mapping.data() - base.data())};
        if (!util::IsPageAligned(offset) || !util::IsPageAligned(mapping.size())) [[unlikely]]
            throw exception("Mapping is not aligned to a page: 0x{:X}-0x{:X} (0x{:X})", mapping.data(), mapping.end().base(), offset);

        auto mirror{mremap(mapping.data(), 0, mapping.size(), MREMAP_MAYMOVE)};
        if (mirror == MAP_FAILED) [[unlikely]]
            throw exception("Failed to create mirror mapping at 0x{:X}-0x{:X} (0x{:X}): {}", mapping.data(), mapping.end().base(), offset, strerror(errno));

        mprotect(mirror, mapping.size(), PROT_READ | PROT_WRITE);

        return span<u8>{reinterpret_cast<u8 *>(mirror), mapping.size()};
    }

    span<u8> MemoryManager::CreateMirrors(const std::vector<span<u8>> &regions) {
        size_t totalSize{};
        for (const auto &region : regions)
            totalSize += region.size();

        auto mirrorBase{mmap(nullptr, totalSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)}; // Reserve address space for all mirrors
        if (mirrorBase == MAP_FAILED) [[unlikely]]
            throw exception("Failed to create mirror base: {} (0x{:X} bytes)", strerror(errno), totalSize);

        size_t mirrorOffset{};
        for (const auto &region : regions) {
            if (!base.contains(region)) [[unlikely]]
                throw exception("Mapping is outside of VMM base: 0x{:X} - 0x{:X}", region.data(), region.end().base());

            auto offset{static_cast<size_t>(region.data() - base.data())};
            if (!util::IsPageAligned(offset) || !util::IsPageAligned(region.size())) [[unlikely]]
                throw exception("Mapping is not aligned to a page: 0x{:X}-0x{:X} (0x{:X})", region.data(), region.end().base(), offset);

            auto mirror{mremap(region.data(), 0, region.size(), MREMAP_FIXED | MREMAP_MAYMOVE, reinterpret_cast<u8 *>(mirrorBase) + mirrorOffset)};
            if (mirror == MAP_FAILED) [[unlikely]]
                throw exception("Failed to create mirror mapping at 0x{:X}-0x{:X} (0x{:X}): {}", region.data(), region.end().base(), offset, strerror(errno));

            mprotect(mirror, region.size(), PROT_READ | PROT_WRITE);

            mirrorOffset += region.size();
        }

        if (mirrorOffset != totalSize) [[unlikely]]
            throw exception("Mirror size mismatch: 0x{:X} != 0x{:X}", mirrorOffset, totalSize);

        return span<u8>{reinterpret_cast<u8 *>(mirrorBase), totalSize};
    }

    void MemoryManager::SetLockOnChunks(span<u8> memory, bool value) {
        std::unique_lock lock(mutex);

        ForeachChunkinRange(memory, [&](std::pair<u8 *, ChunkDescriptor> &desc) __attribute__((always_inline)) {
            desc.second.attributes.isBorrowed = value;
            MapInternal(&desc);
        });
    }

    void MemoryManager::SetCPUCachingOnChunks(span<u8> memory, bool value) {
        std::unique_lock lock(mutex);

        ForeachChunkinRange(memory, [&](std::pair<u8 *, ChunkDescriptor> &desc) __attribute__((always_inline)) {
            desc.second.attributes.isUncached = value;
            MapInternal(&desc);
        });
    }

    void MemoryManager::SetChunkPermission(span<u8> memory, memory::Permission permission) {
        std::unique_lock lock(mutex);

        ForeachChunkinRange(memory, [&](std::pair<u8 *, ChunkDescriptor> &desc) __attribute__((always_inline)) {
            desc.second.permission = permission;
            MapInternal(&desc);
        });
    }

    std::optional<std::pair<u8 *, ChunkDescriptor>> MemoryManager::GetChunk(u8 *addr) {
        std::shared_lock lock(mutex);

        if (!addressSpace.contains(addr)) [[unlikely]]
            return std::nullopt;

        auto chunkBase = upper_bound(addr);
        if (addr < chunkBase->first) [[likely]]
            --chunkBase;

        return std::make_optional(*chunkBase);
    }

    __attribute__((always_inline)) void MemoryManager::MapCodeMemory(span<u8> memory, memory::Permission permission) {
        std::unique_lock lock(mutex);

        std::pair<u8 *, ChunkDescriptor> temp(
            memory.data(),
            ChunkDescriptor{
                .size = memory.size(),
                .permission = permission,
                .state = memory::states::Code});

        MapInternal(&temp);
    }

    __attribute__((always_inline)) void MemoryManager::MapMutableCodeMemory(span<u8> memory) {
        std::unique_lock lock(mutex);

        std::pair<u8 *, ChunkDescriptor> temp(
            memory.data(),
            ChunkDescriptor{
                .size = memory.size(),
                .permission = {true, true, false},
                .state = memory::states::CodeMutable});

        MapInternal(&temp);
    }

    __attribute__((always_inline)) void MemoryManager::MapStackMemory(span<u8> memory) {
        std::unique_lock lock(mutex);

        std::pair<u8 *, ChunkDescriptor> temp(
            memory.data(),
            ChunkDescriptor{
                .size = memory.size(),
                .permission = {true, true, false},
                .state = memory::states::Stack,
                .isSrcMergeDisallowed = true});

        MapInternal(&temp);
    }

    __attribute__((always_inline)) void MemoryManager::MapHeapMemory(span<u8> memory) {
        std::unique_lock lock(mutex);

        std::pair<u8 *, ChunkDescriptor> temp(
            memory.data(),
            ChunkDescriptor{
                .size = memory.size(),
                .permission = {true, true, false},
                .state = memory::states::Heap});

        MapInternal(&temp);
    }

    __attribute__((always_inline)) void MemoryManager::MapSharedMemory(span<u8> memory, memory::Permission permission) {
        std::unique_lock lock(mutex);

        std::pair<u8 *, ChunkDescriptor> temp(
            memory.data(),
            ChunkDescriptor{
                .size = memory.size(),
                .permission = permission,
                .state = memory::states::SharedMemory,
                .isSrcMergeDisallowed = true});

        MapInternal(&temp);
    }

    __attribute__((always_inline)) void MemoryManager::MapTransferMemory(span<u8> memory, memory::Permission permission) {
        std::unique_lock lock(mutex);

        std::pair<u8 *, ChunkDescriptor> temp(
            memory.data(),
            ChunkDescriptor{
                .size = memory.size(),
                .permission = permission,
                .state = permission.raw ? memory::states::TransferMemory : memory::states::TransferMemoryIsolated,
                .isSrcMergeDisallowed = true});

        MapInternal(&temp);
    }

    __attribute__((always_inline)) void MemoryManager::MapThreadLocalMemory(span<u8> memory) {
        std::unique_lock lock(mutex);

        std::pair<u8 *, ChunkDescriptor> temp(
            memory.data(),
            ChunkDescriptor{
                .size = memory.size(),
                .permission = {true, true, false},
                .state = memory::states::ThreadLocal});
        MapInternal(&temp);
    }

    __attribute__((always_inline)) void MemoryManager::Reserve(span<u8> memory) {
        std::unique_lock lock(mutex);

        std::pair<u8 *, ChunkDescriptor> temp(
            memory.data(),
            ChunkDescriptor{
                .size = memory.size(),
                .permission = {false, false, false},
                .state = memory::states::Reserved});
        MapInternal(&temp);
    }

    __attribute__((always_inline)) void MemoryManager::UnmapMemory(span<u8> memory) {
        std::unique_lock lock(mutex);

        ForeachChunkinRange(memory, [&](std::pair<u8 *, ChunkDescriptor> &desc) {
            if (desc.second.state != memory::states::Unmapped)
                FreeMemory(span<u8>((u8 *)desc.first, desc.second.size));
        });

        std::pair<u8 *, ChunkDescriptor> temp(
            memory.data(),
            ChunkDescriptor{
            .size = memory.size(),
            .permission = {false, false, false},
            .state = memory::states::Unmapped});
        MapInternal(&temp);
    }

    __attribute__((always_inline)) void MemoryManager::FreeMemory(span<u8> memory) {
        u8 *alignedStart{util::AlignUp(memory.data(), constant::PageSize)};
        u8 *alignedEnd{util::AlignDown(memory.end().base(), constant::PageSize)};

        if (alignedStart < alignedEnd) [[likely]]
            if (madvise(alignedStart, static_cast<size_t>(alignedEnd - alignedStart), MADV_REMOVE) == -1) [[unlikely]]
                Logger::Error("Failed to free memory: {}", strerror(errno));
    }

    void MemoryManager::AddRef(const std::shared_ptr<type::KMemory> &ptr) {
        memRefs.push_back(ptr);
    }

    void MemoryManager::RemoveRef(const std::shared_ptr<type::KMemory> &ptr) {
        std::vector<std::shared_ptr<type::KMemory>>::iterator i{std::find(memRefs.begin(), memRefs.end(), ptr)};

        if (*i == ptr) {
            memRefs.erase(i);
        }
    }

    size_t MemoryManager::GetUserMemoryUsage() {
        std::shared_lock lock(mutex);
        size_t size{};

        for (auto &chunk : chunks) {
            if (chunk.second.state == memory::states::Heap)
                size += chunk.second.size;
        }

        return size + code.size() + state.process->mainThreadStack.size();
    }

    size_t MemoryManager::GetSystemResourceUsage() {
        std::shared_lock lock(mutex);
        constexpr size_t KMemoryBlockSize{0x40};
        return std::min(static_cast<size_t>(state.process->npdm.meta.systemResourceSize), util::AlignUp(chunks.size() * KMemoryBlockSize, constant::PageSize));
    }
}
