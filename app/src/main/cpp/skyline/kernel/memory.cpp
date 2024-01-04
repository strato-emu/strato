// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <fstream>
#include <asm-generic/unistd.h>
#include <fcntl.h>
#include "memory.h"
#include "types/KProcess.h"

namespace skyline::kernel {
    MemoryManager::MemoryManager(const DeviceState &state) noexcept : state{state}, processHeapSize{}, memRefs{} {}

    MemoryManager::~MemoryManager() noexcept {
        if (base.valid() && !base.empty())
            munmap(reinterpret_cast<void *>(base.data()), base.size());
        if (addressSpaceType != memory::AddressSpaceType::AddressSpace39Bit)
            if (codeBase36Bit.valid() && !codeBase36Bit.empty())
                munmap(reinterpret_cast<void *>(codeBase36Bit.data()), codeBase36Bit.size());
    }

    void MemoryManager::MapInternal(const std::pair<u8 *, ChunkDescriptor> &newDesc) {
        // The chunk that contains / precedes the new chunk base address
        auto firstChunkBase{chunks.lower_bound(newDesc.first)};
        if (newDesc.first <= firstChunkBase->first)
            --firstChunkBase;

        // The chunk that contains / follows the end address of the new chunk
        auto lastChunkBase{chunks.lower_bound(newDesc.first + newDesc.second.size)};
        if ((newDesc.first + newDesc.second.size) < lastChunkBase->first)
            --lastChunkBase;

        ChunkDescriptor firstChunk{firstChunkBase->second};
        ChunkDescriptor lastChunk{lastChunkBase->second};

        bool needsReprotection{false};
        bool isUnmapping{newDesc.second.state == memory::states::Unmapped};

        // We cut a hole in a single chunk
        if (firstChunkBase->first == lastChunkBase->first) {
            if (firstChunk.IsCompatible(newDesc.second)) [[unlikely]]
                // No editing necessary
                return;

            if ((firstChunk.state == memory::states::Unmapped) != isUnmapping)
                needsReprotection = true;

            // We reduce the size of the first half
            firstChunk.size = static_cast<size_t>(newDesc.first - firstChunkBase->first);
            chunks[firstChunkBase->first] = firstChunk;

            // We create the chunk's second half
            lastChunk.size = static_cast<size_t>((lastChunkBase->first + lastChunk.size) - (newDesc.first + newDesc.second.size));
            chunks.insert({newDesc.first + newDesc.second.size, lastChunk});

            // Insert new chunk in between
            chunks.insert(newDesc);
        } else {
            // If there are descriptors between first and last chunk, delete them
            if ((firstChunkBase->first + firstChunk.size) != lastChunkBase->first) {
                auto tempChunkBase{std::next(firstChunkBase)};

                while (tempChunkBase->first != lastChunkBase->first) {
                    auto tmp{tempChunkBase++};
                    if ((tmp->second.state == memory::states::Unmapped) != isUnmapping)
                        needsReprotection = true;
                }
                chunks.erase(std::next(firstChunkBase), lastChunkBase);
            }

            bool shouldInsert{true};

            // We check if the new chunk and the first chunk is mergable
            if (firstChunk.IsCompatible(newDesc.second)) {
                shouldInsert = false;

                firstChunk.size = static_cast<size_t>((newDesc.first + newDesc.second.size) - firstChunkBase->first);
                chunks[firstChunkBase->first] = firstChunk;
            } else if ((firstChunkBase->first + firstChunk.size) != newDesc.first) { // If it's not mergable check if it needs resizing
                firstChunk.size = static_cast<size_t>(newDesc.first - firstChunkBase->first);

                chunks[firstChunkBase->first] = firstChunk;

                if ((firstChunk.state == memory::states::Unmapped) != isUnmapping)
                    needsReprotection = true;
            }

            // We check if the new chunk and the last chunk is mergable
            if (lastChunk.IsCompatible(newDesc.second)) {
                u8 *oldBase{lastChunkBase->first};
                chunks.erase(lastChunkBase);

                if (shouldInsert) {
                    shouldInsert = false;

                    lastChunk.size = static_cast<size_t>((lastChunk.size + oldBase) - (newDesc.first));

                    chunks[newDesc.first] = lastChunk;
                } else {
                    firstChunk.size = static_cast<size_t>((lastChunk.size + oldBase) - firstChunkBase->first);
                    chunks[firstChunkBase->first] = firstChunk;
                }
            } else if ((newDesc.first + newDesc.second.size) != lastChunkBase->first) { // If it's not mergable check if it needs resizing
                lastChunk.size = static_cast<size_t>((lastChunk.size + lastChunkBase->first) - (newDesc.first + newDesc.second.size));

                chunks.erase(lastChunkBase);
                chunks[newDesc.first + newDesc.second.size] = lastChunk;

                if ((lastChunk.state == memory::states::Unmapped) != isUnmapping)
                    needsReprotection = true;
            }

            // Insert if not merged
            if (shouldInsert)
                chunks.insert(newDesc);
        }

        if (needsReprotection)
            if (mprotect(newDesc.first, newDesc.second.size, !isUnmapping ? PROT_READ | PROT_WRITE | PROT_EXEC : PROT_NONE)) [[unlikely]]
                LOGW("Reprotection failed: {}", strerror(errno));
    }

    void MemoryManager::ForeachChunkInRange(span<u8> memory, auto editCallback) {
        auto chunkBase{chunks.lower_bound(memory.data())};
        if (memory.data() < chunkBase->first)
            --chunkBase;

        size_t sizeLeft{memory.size()};

        if (chunkBase->first < memory.data()) [[unlikely]] {
            size_t chunkSize{std::min<size_t>(chunkBase->second.size - (static_cast<size_t>(memory.data() - chunkBase->first)), memory.size())};

            std::pair<u8 *, ChunkDescriptor> temp{memory.data(), chunkBase->second};
            temp.second.size = chunkSize;
            editCallback(temp);

            ++chunkBase;
            sizeLeft -= chunkSize;
        }

        while (sizeLeft) {
            if (sizeLeft < chunkBase->second.size) {
                std::pair<u8 *, ChunkDescriptor> temp(*chunkBase);
                temp.second.size = sizeLeft;
                editCallback(temp);
                break;
            } else [[likely]] {
                std::pair<u8 *, ChunkDescriptor> temp(*chunkBase);

                editCallback(temp);

                sizeLeft = sizeLeft - chunkBase->second.size;
                ++chunkBase;
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

        base = AllocateMappedRange(baseSize, RegionAlignment, KgslReservedRegionSize, addressSpace.size(), false);

        if (type != memory::AddressSpaceType::AddressSpace36Bit) {
            code = base;
        } else {
            code = codeBase36Bit = AllocateMappedRange(0x78000000, RegionAlignment, 0x8000000, KgslReservedRegionSize, false);

            if ((reinterpret_cast<u64>(base.data()) + baseSize) > (1ULL << 36)) {
                LOGW("Couldn't fit regions into 36 bit AS! Resizing AS to 39 bits!");
                addressSpace = span<u8>{reinterpret_cast<u8 *>(0), 1ULL << 39};
            }
        }

        // Insert a placeholder element at the end of the map to make sure upper_bound/lower_bound never triggers std::map::end() which is broken
        chunks = {{addressSpace.data(),{
            .size = addressSpace.size(),
            .state = memory::states::Unmapped,
        }}, {reinterpret_cast<u8 *>(UINT64_MAX), {
            .state = memory::states::Reserved,
        }}};
    }

    void MemoryManager::InitializeRegions(span<u8> codeRegion) {
        if (!util::IsAligned(codeRegion.data(), RegionAlignment)) [[unlikely]]
            throw exception("Non-aligned code region was used to initialize regions: {} - {}", fmt::ptr(codeRegion.data()), fmt::ptr(codeRegion.end().base()));

        switch (addressSpaceType) {
            case memory::AddressSpaceType::AddressSpace36Bit: {

                // As a workaround if we can't place the code region at the base of the AS we mark it as inaccessible heap so rtld doesn't crash
                if (codeBase36Bit.data() != reinterpret_cast<u8 *>(0x8000000)) {
                    MapInternal(std::pair<u8 *, ChunkDescriptor>(reinterpret_cast<u8 *>(0x8000000),{
                        .size = reinterpret_cast<size_t>(codeBase36Bit.data() - 0x8000000),
                        .state = memory::states::Heap
                    }));
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

                if (newSize != base.size()) [[likely]]
                    munmap(base.end().base(), newSize - base.size());

                break;
            }

            default:
                throw exception("Regions initialized without VMM initialization");
        }

        if (codeRegion.size() > code.size()) [[unlikely]]
            throw exception("Code region ({}) is smaller than mapped code size ({})", code.size(), codeRegion.size());

        LOGD("Region Map:\n"
             "VMM Base: {}\n"
             "Code Region: {} - {} (Size: 0x{:X})\n"
             "Alias Region: {} - {} (Size: 0x{:X})\n"
             "Heap Region: {} - {} (Size: 0x{:X})\n"
             "Stack Region: {} - {} (Size: 0x{:X})\n"
             "TLS/IO Region: {} - {} (Size: 0x{:X})",
             fmt::ptr(code.data()),
             fmt::ptr(code.data()), fmt::ptr(code.end().base()), code.size(),
             fmt::ptr(alias.data()), fmt::ptr(alias.end().base()), alias.size(),
             fmt::ptr(heap.data()), fmt::ptr(heap.end().base()), heap.size(),
             fmt::ptr(stack.data()), fmt::ptr(stack.end().base()), stack.size(),
             fmt::ptr(tlsIo.data()), fmt::ptr(tlsIo.end().base()), tlsIo.size());
    }

    span<u8> MemoryManager::CreateMirror(span<u8> mapping) {
        if (!base.contains(mapping)) [[unlikely]]
            throw exception("Mapping is outside of VMM base: {} - {}", fmt::ptr(mapping.data()), fmt::ptr(mapping.end().base()));

        auto offset{static_cast<size_t>(mapping.data() - base.data())};
        if (!util::IsPageAligned(offset) || !util::IsPageAligned(mapping.size())) [[unlikely]]
            throw exception("Mapping is not aligned to a page: {} - {} (0x{:X})", fmt::ptr(mapping.data()), fmt::ptr(mapping.end().base()), offset);

        auto mirror{mremap(mapping.data(), 0, mapping.size(), MREMAP_MAYMOVE)};
        if (mirror == MAP_FAILED) [[unlikely]]
            throw exception("Failed to create mirror mapping at {} - {} (0x{:X}): {}", fmt::ptr(mapping.data()), fmt::ptr(mapping.end().base()), offset, strerror(errno));

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
                throw exception("Mapping is outside of VMM base: {} - {}", fmt::ptr(region.data()), fmt::ptr(region.end().base()));

            auto offset{static_cast<size_t>(region.data() - base.data())};
            if (!util::IsPageAligned(offset) || !util::IsPageAligned(region.size())) [[unlikely]]
                throw exception("Mapping is not aligned to a page: {} - {} (0x{:X})", fmt::ptr(region.data()), fmt::ptr(region.end().base()), offset);

            auto mirror{mremap(region.data(), 0, region.size(), MREMAP_FIXED | MREMAP_MAYMOVE, reinterpret_cast<u8 *>(mirrorBase) + mirrorOffset)};
            if (mirror == MAP_FAILED) [[unlikely]]
                throw exception("Failed to create mirror mapping at {} - {} (0x{:X}): {}", fmt::ptr(region.data()), fmt::ptr(region.end().base()), offset, strerror(errno));

            mprotect(mirror, region.size(), PROT_READ | PROT_WRITE);

            mirrorOffset += region.size();
        }

        if (mirrorOffset != totalSize) [[unlikely]]
            throw exception("Mirror size mismatch: 0x{:X} != 0x{:X}", mirrorOffset, totalSize);

        return span<u8>{reinterpret_cast<u8 *>(mirrorBase), totalSize};
    }

    void MemoryManager::SetRegionBorrowed(span<u8> memory, bool value) {
        std::unique_lock lock{mutex};

        ForeachChunkInRange(memory, [&](std::pair<u8 *, ChunkDescriptor> &desc) __attribute__((always_inline)) {
            desc.second.attributes.isBorrowed = value;
            MapInternal(desc);
        });
    }

    void MemoryManager::SetRegionCpuCaching(span<u8> memory, bool value) {
        std::unique_lock lock{mutex};

        ForeachChunkInRange(memory, [&](std::pair<u8 *, ChunkDescriptor> &desc) __attribute__((always_inline)) {
            desc.second.attributes.isUncached = value;
            MapInternal(desc);
        });
    }

    void MemoryManager::SetRegionPermission(span<u8> memory, memory::Permission permission) {
        std::unique_lock lock{mutex};

        ForeachChunkInRange(memory, [&](std::pair<u8 *, ChunkDescriptor> &desc) __attribute__((always_inline)) {
            desc.second.permission = permission;
            MapInternal(desc);
        });
    }

    std::optional<std::pair<u8 *, ChunkDescriptor>> MemoryManager::GetChunk(u8 *addr) {
        std::shared_lock lock{mutex};

        if (!addressSpace.contains(addr)) [[unlikely]]
            return std::nullopt;

        auto chunkBase{chunks.lower_bound(addr)};
        if (addr < chunkBase->first)
            --chunkBase;

        return std::make_optional(*chunkBase);
    }

    __attribute__((always_inline)) void MemoryManager::MapCodeMemory(span<u8> memory, memory::Permission permission) {
        std::unique_lock lock{mutex};

        MapInternal(std::pair<u8 *, ChunkDescriptor>(
            memory.data(),{
                .size = memory.size(),
                .permission = permission,
                .state = memory::states::Code
        }));
    }

    __attribute__((always_inline)) void MemoryManager::MapMutableCodeMemory(span<u8> memory) {
        std::unique_lock lock{mutex};

        MapInternal(std::pair<u8 *, ChunkDescriptor>(
            memory.data(),{
                .size = memory.size(),
                .permission = {true, true, false},
                .state = memory::states::CodeMutable
        }));
    }

    __attribute__((always_inline)) void MemoryManager::MapStackMemory(span<u8> memory) {
        std::unique_lock lock{mutex};

        MapInternal(std::pair<u8 *, ChunkDescriptor>(
            memory.data(),{
                .size = memory.size(),
                .permission = {true, true, false},
                .state = memory::states::Stack,
                .isSrcMergeDisallowed = true
        }));
    }

    __attribute__((always_inline)) void MemoryManager::MapHeapMemory(span<u8> memory) {
        std::unique_lock lock{mutex};

        MapInternal(std::pair<u8 *, ChunkDescriptor>(
            memory.data(),{
                .size = memory.size(),
                .permission = {true, true, false},
                .state = memory::states::Heap
        }));
    }

    __attribute__((always_inline)) void MemoryManager::MapSharedMemory(span<u8> memory, memory::Permission permission) {
        std::unique_lock lock{mutex};

        MapInternal(std::pair<u8 *, ChunkDescriptor>(
            memory.data(),{
                .size = memory.size(),
                .permission = permission,
                .state = memory::states::SharedMemory,
                .isSrcMergeDisallowed = true
        }));
    }

    __attribute__((always_inline)) void MemoryManager::MapTransferMemory(span<u8> memory, memory::Permission permission) {
        std::unique_lock lock{mutex};

        MapInternal(std::pair<u8 *, ChunkDescriptor>(
            memory.data(),{
                .size = memory.size(),
                .permission = permission,
                .state = permission.raw ? memory::states::TransferMemory : memory::states::TransferMemoryIsolated,
                .isSrcMergeDisallowed = true
        }));
    }

    __attribute__((always_inline)) void MemoryManager::MapThreadLocalMemory(span<u8> memory) {
        std::unique_lock lock{mutex};

        MapInternal(std::pair<u8 *, ChunkDescriptor>(
            memory.data(),{
                .size = memory.size(),
                .permission = {true, true, false},
                .state = memory::states::ThreadLocal
        }));
    }

    __attribute__((always_inline)) void MemoryManager::Reserve(span<u8> memory) {
        std::unique_lock lock{mutex};

        MapInternal(std::pair<u8 *, ChunkDescriptor>(
            memory.data(),{
                .size = memory.size(),
                .permission = {false, false, false},
                .state = memory::states::Reserved
        }));
    }

    __attribute__((always_inline)) void MemoryManager::UnmapMemory(span<u8> memory) {
        std::unique_lock lock{mutex};

        ForeachChunkInRange(memory, [&](const std::pair<u8 *, ChunkDescriptor> &desc) {
            if (desc.second.state != memory::states::Unmapped)
                FreeMemory(span<u8>(desc.first, desc.second.size));
        });

        MapInternal(std::pair<u8 *, ChunkDescriptor>(
            memory.data(),{
                .size = memory.size(),
                .permission = {false, false, false},
                .state = memory::states::Unmapped
        }));
    }

    __attribute__((always_inline)) void MemoryManager::FreeMemory(span<u8> memory) {
        u8 *alignedStart{util::AlignUp(memory.data(), constant::PageSize)};
        u8 *alignedEnd{util::AlignDown(memory.end().base(), constant::PageSize)};

        if (alignedStart < alignedEnd) [[likely]]
            if (madvise(alignedStart, static_cast<size_t>(alignedEnd - alignedStart), MADV_REMOVE) == -1) [[unlikely]]
                LOGE("Failed to free memory: {}", strerror(errno));
    }

    void MemoryManager::SvcMapMemory(span<u8> source, span<u8> destination) {
        std::unique_lock lock{mutex};

        MapInternal(std::pair<u8 *, ChunkDescriptor>(
            destination.data(),{
                .size = destination.size(),
                .permission = {true, true, false},
                .state = memory::states::Stack,
                .isSrcMergeDisallowed = true
        }));

        std::memcpy(destination.data(), source.data(), source.size());

        ForeachChunkInRange(source, [&](std::pair<u8 *, ChunkDescriptor> &desc) __attribute__((always_inline)) {
            desc.second.permission = {false, false, false};
            desc.second.attributes.isBorrowed = true;
            MapInternal(desc);
        });
    }

    void MemoryManager::SvcUnmapMemory(span<u8> source, span<u8> destination) {
        std::unique_lock lock{mutex};

        auto dstChunk = chunks.lower_bound(destination.data());
        if (destination.data() < dstChunk->first)
            --dstChunk;
        while (dstChunk->second.state == memory::states::Unmapped)
            ++dstChunk;

        if ((destination.data() + destination.size()) > dstChunk->first) [[likely]] {
            ForeachChunkInRange(span<u8>{source.data() + (dstChunk->first - destination.data()), dstChunk->second.size}, [&](std::pair<u8 *, ChunkDescriptor> &desc) __attribute__((always_inline)) {
                desc.second.permission = dstChunk->second.permission;
                desc.second.attributes.isBorrowed = false;
                MapInternal(desc);
            });

            std::memcpy(source.data() + (dstChunk->first - destination.data()), dstChunk->first, dstChunk->second.size);
        }
    }

    void MemoryManager::AddRef(std::shared_ptr<type::KMemory> ptr) {
        memRefs.push_back(std::move(ptr));
    }

    void MemoryManager::RemoveRef(std::shared_ptr<type::KMemory> ptr) {
        auto i = std::find(memRefs.begin(), memRefs.end(), ptr);

        if (*i == ptr) [[likely]]
            memRefs.erase(i);
    }

    size_t MemoryManager::GetUserMemoryUsage() {
        std::shared_lock lock{mutex};
        size_t size{};

        auto currChunk = chunks.lower_bound(heap.data());

        while (currChunk->first < heap.end().base()) {
            if (currChunk->second.state == memory::states::Heap)
                size += currChunk->second.size;
            ++currChunk;
        }

        return size + code.size() + state.process->mainThreadStack.size();
    }

    size_t MemoryManager::GetSystemResourceUsage() {
        std::shared_lock lock{mutex};
        constexpr size_t KMemoryBlockSize{0x40};
        return std::min(static_cast<size_t>(state.process->npdm.meta.systemResourceSize), util::AlignUp(chunks.size() * KMemoryBlockSize, constant::PageSize));
    }
}
