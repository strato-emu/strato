// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include "megabuffer.h"

namespace skyline::gpu {
    MegaBufferChunk::MegaBufferChunk(GPU &gpu) : backing{gpu.memory.AllocateBuffer(MegaBufferChunkSize)}, freeRegion{backing.subspan(PAGE_SIZE)} {}

    bool MegaBufferChunk::TryReset() {
        if (cycle && cycle->Poll(true)) {
            freeRegion = backing.subspan(PAGE_SIZE);
            cycle = nullptr;
            return true;
        }

        return cycle == nullptr;
    }

    vk::Buffer MegaBufferChunk::GetBacking() const {
        return backing.vkBuffer;
    }

    std::pair<vk::DeviceSize, span<u8>> MegaBufferChunk::Allocate(const std::shared_ptr<FenceCycle> &newCycle, vk::DeviceSize size, bool pageAlign) {
        if (pageAlign) {
            // If page aligned data was requested then align the free
            auto alignedFreeBase{util::AlignUp(static_cast<size_t>(freeRegion.data() - backing.data()), PAGE_SIZE)};
            freeRegion = backing.subspan(alignedFreeBase);
        }

        if (size > freeRegion.size())
            return {0, {}};

        if (cycle != newCycle) {
            newCycle->ChainCycle(cycle);
            cycle = newCycle;
        }

        // Allocate space for data from the free region
        auto resultSpan{freeRegion.subspan(0, size)};

        // Move the free region along
        freeRegion = freeRegion.subspan(size);

        return {static_cast<vk::DeviceSize>(resultSpan.data() - backing.data()), resultSpan};
    }

    MegaBufferAllocator::MegaBufferAllocator(GPU &gpu) : gpu{gpu}, activeChunk{chunks.emplace(chunks.end(), gpu)} {}

    MegaBufferAllocator::Allocation MegaBufferAllocator::Allocate(const std::shared_ptr<FenceCycle> &cycle, vk::DeviceSize size, bool pageAlign) {
        if (auto allocation{activeChunk->Allocate(cycle, size, pageAlign)}; allocation.first)
            return {activeChunk->GetBacking(), allocation.first, allocation.second};

        activeChunk = ranges::find_if(chunks, [&](auto &chunk) { return chunk.TryReset(); });
        if (activeChunk == chunks.end()) // If there are no chunks available, allocate a new one
            activeChunk = chunks.emplace(chunks.end(), gpu);

        if (auto allocation{activeChunk->Allocate(cycle, size, pageAlign)}; allocation.first)
            return {activeChunk->GetBacking(), allocation.first, allocation.second};
        else
            throw exception("Failed to to allocate megabuffer space for size: 0x{:X}", size);
    }

    MegaBufferAllocator::Allocation MegaBufferAllocator::Push(const std::shared_ptr<FenceCycle> &cycle, span<u8> data, bool pageAlign) {
        auto allocation{Allocate(cycle, data.size(), pageAlign)};
        allocation.region.copy_from(data);
        return allocation;
    }
}
