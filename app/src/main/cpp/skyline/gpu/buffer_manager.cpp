// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>

#include "buffer_manager.h"

namespace skyline::gpu {
    MegaBuffer::MegaBuffer(GPU &gpu) : backing(gpu.memory.AllocateBuffer(Size)), freeRegion(backing.subspan(PAGE_SIZE)) {}

    void MegaBuffer::Reset() {
        std::scoped_lock lock{mutex};
        freeRegion = backing.subspan(PAGE_SIZE);
    }

    vk::Buffer MegaBuffer::GetBacking() const {
        return backing.vkBuffer;
    }

    vk::DeviceSize MegaBuffer::Push(span<u8> data, bool pageAlign) {
        std::scoped_lock lock{mutex};

        if (data.size() > freeRegion.size())
            throw exception("Ran out of megabuffer space! Alloc size: 0x{:X}", data.size());

        if (pageAlign) {
            // If page aligned data was requested then align the free
            auto alignedFreeBase{util::AlignUp(static_cast<size_t>(freeRegion.data() - backing.data()), PAGE_SIZE)};
            freeRegion = backing.subspan(alignedFreeBase);
        }

        // Allocate space for data from the free region
        auto resultSpan{freeRegion.subspan(0, data.size())};
        resultSpan.copy_from(data);

        // Move the free region along
        freeRegion = freeRegion.subspan(data.size());
        return static_cast<vk::DeviceSize>(resultSpan.data() - backing.data());
    }

    BufferManager::BufferManager(GPU &gpu) : gpu(gpu), megaBuffer(gpu) {}

    bool BufferManager::BufferLessThan(const std::shared_ptr<Buffer> &it, u8 *pointer) {
        return it->guest->begin().base() < pointer;
    }

    BufferView BufferManager::FindOrCreate(GuestBuffer guestMapping, const std::shared_ptr<FenceCycle> &cycle) {
        /*
         * We align the buffer to the page boundary to ensure that:
         * 1) Any buffer view has the same alignment guarantees as on the guest, this is required for UBOs, SSBOs and Texel buffers
         * 2) We can coalesce a lot of tiny buffers into a single large buffer covering an entire page, this is often the case for index buffers and vertex buffers
         */
        auto alignedStart{util::AlignDown(guestMapping.begin().base(), PAGE_SIZE)}, alignedEnd{util::AlignUp(guestMapping.end().base(), PAGE_SIZE)};
        vk::DeviceSize offset{static_cast<size_t>(guestMapping.begin().base() - alignedStart)}, size{guestMapping.size()};
        guestMapping = span<u8>{alignedStart, alignedEnd};

        std::scoped_lock lock(mutex);

        // Lookup for any buffers overlapping with the supplied guest mapping
        boost::container::small_vector<std::shared_ptr<Buffer>, 4> overlaps;
        for (auto entryIt{std::lower_bound(buffers.begin(), buffers.end(), guestMapping.end().base(), BufferLessThan)}; entryIt != buffers.begin() && (*--entryIt)->guest->begin() <= guestMapping.end();)
            if ((*entryIt)->guest->end() > guestMapping.begin())
                overlaps.push_back(*entryIt);

        if (overlaps.size() == 1) [[likely]] {
            auto buffer{overlaps.front()};
            if (buffer->guest->begin() <= guestMapping.begin() && buffer->guest->end() >= guestMapping.end()) {
                // If we find a buffer which can entirely fit the guest mapping, we can just return a view into it
                std::scoped_lock bufferLock{*buffer};
                return buffer->GetView(static_cast<vk::DeviceSize>(guestMapping.begin() - buffer->guest->begin()) + offset, size);
            }
        }

        // Find the extents of the new buffer we want to create that can hold all overlapping buffers
        auto lowestAddress{guestMapping.begin().base()}, highestAddress{guestMapping.end().base()};
        for (const auto &overlap : overlaps) {
            auto mapping{*overlap->guest};
            if (mapping.begin().base() < lowestAddress)
                lowestAddress = mapping.begin().base();
            if (mapping.end().base() > highestAddress)
                highestAddress = mapping.end().base();
        }

        auto newBuffer{std::make_shared<Buffer>(gpu, cycle, span<u8>(lowestAddress, highestAddress), overlaps)};
        for (auto &overlap : overlaps) {
            std::scoped_lock overlapLock{*overlap};

            buffers.erase(std::find(buffers.begin(), buffers.end(), overlap));

            // Transfer all views from the overlapping buffer to the new buffer with the new buffer and updated offset
            vk::DeviceSize overlapOffset{static_cast<vk::DeviceSize>(overlap->guest->begin() - newBuffer->guest->begin())};
            if (overlapOffset != 0)
                for (auto &view : overlap->views)
                    view.offset += overlapOffset;

            newBuffer->views.splice(newBuffer->views.end(), overlap->views);

            // Transfer all delegates references from the overlapping buffer to the new buffer
            for (auto &delegate : overlap->delegates) {
                atomic_exchange(&delegate->buffer, newBuffer);
                if (delegate->usageCallback)
                    delegate->usageCallback(*delegate->view, newBuffer);
            }

            newBuffer->delegates.splice(newBuffer->delegates.end(), overlap->delegates);
        }

        buffers.insert(std::lower_bound(buffers.begin(), buffers.end(), newBuffer->guest->end().base(), BufferLessThan), newBuffer);

        return newBuffer->GetView(static_cast<vk::DeviceSize>(guestMapping.begin() - newBuffer->guest->begin()) + offset, size);
    }
}
