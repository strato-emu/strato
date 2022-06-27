// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>

#include "buffer_manager.h"

namespace skyline::gpu {
    BufferManager::BufferManager(GPU &gpu) : gpu(gpu) {}

    bool BufferManager::BufferLessThan(const std::shared_ptr<Buffer> &it, u8 *pointer) {
        return it->guest->begin().base() < pointer;
    }

    void BufferManager::lock() {
        mutex.lock();
    }

    void BufferManager::unlock() {
        mutex.unlock();
    }

    bool BufferManager::try_lock() {
        return mutex.try_lock();
    }

    BufferView BufferManager::FindOrCreate(GuestBuffer guestMapping, ContextTag tag) {
        /*
         * We align the buffer to the page boundary to ensure that:
         * 1) Any buffer view has the same alignment guarantees as on the guest, this is required for UBOs, SSBOs and Texel buffers
         * 2) We can coalesce a lot of tiny buffers into a single large buffer covering an entire page, this is often the case for index buffers and vertex buffers
         */
        auto alignedStart{util::AlignDown(guestMapping.begin().base(), PAGE_SIZE)}, alignedEnd{util::AlignUp(guestMapping.end().base(), PAGE_SIZE)};
        vk::DeviceSize offset{static_cast<size_t>(guestMapping.begin().base() - alignedStart)}, size{guestMapping.size()};
        guestMapping = span<u8>{alignedStart, alignedEnd};

        // Lookup for any buffers overlapping with the supplied guest mapping
        boost::container::small_vector<std::shared_ptr<Buffer>, 4> overlaps;
        for (auto entryIt{std::lower_bound(buffers.begin(), buffers.end(), guestMapping.end().base(), BufferLessThan)}; entryIt != buffers.begin() && (*--entryIt)->guest->begin() <= guestMapping.end();)
            if ((*entryIt)->guest->end() > guestMapping.begin())
                overlaps.push_back(*entryIt);

        if (overlaps.size() == 1) [[likely]] {
            auto buffer{overlaps.front()};
            if (buffer->guest->begin() <= guestMapping.begin() && buffer->guest->end() >= guestMapping.end()) {
                // If we find a buffer which can entirely fit the guest mapping, we can just return a view into it
                ContextLock bufferLock{tag, *buffer};
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

        auto newBuffer{std::make_shared<Buffer>(gpu, span<u8>{lowestAddress, highestAddress}, tag, overlaps)};
        for (auto &overlap : overlaps) {
            ContextLock overlapLock{tag, *overlap};

            buffers.erase(std::find(buffers.begin(), buffers.end(), overlap));

            // Transfer all views from the overlapping buffer to the new buffer with the new buffer and updated offset, ensuring pointer stability
            vk::DeviceSize overlapOffset{static_cast<vk::DeviceSize>(overlap->guest->begin() - newBuffer->guest->begin())};
            for (auto it{overlap->views.begin()}; it != overlap->views.end(); it++) {
                if (overlapOffset)
                    // This is a slight hack as we really shouldn't be changing the underlying non-mutable set elements without a rehash but without writing our own set impl this is the best we can do
                    const_cast<Buffer::BufferViewStorage *>(&*it)->offset += overlapOffset;

                // Reset the sequence number to the initial one, if the new buffer was created from any GPU dirty overlaps then the new buffer's sequence will be incremented past this thus forcing a reacquire if necessary
                // This is fine to do in the set since the hash and operator== do not use this value
                it->lastAcquiredSequence = Buffer::InitialSequenceNumber;
            }

            if (overlapOffset)
                // All current hashes are invalidated by above loop if overlapOffset is nonzero so rehash the container
                overlap->views.rehash(0);

            // Merge the view sets, this will keep pointer stability hence avoiding any reallocation
            newBuffer->views.merge(overlap->views);

            // Transfer all delegates references from the overlapping buffer to the new buffer
            for (auto &delegate : overlap->delegates) {
                delegate->buffer = newBuffer;
                if (delegate->usageCallback)
                    delegate->usageCallback(*delegate->view, newBuffer);
            }

            newBuffer->delegates.splice(newBuffer->delegates.end(), overlap->delegates);
        }

        buffers.insert(std::lower_bound(buffers.begin(), buffers.end(), newBuffer->guest->end().base(), BufferLessThan), newBuffer);

        return newBuffer->GetView(static_cast<vk::DeviceSize>(guestMapping.begin() - newBuffer->guest->begin()) + offset, size);
    }

    constexpr static vk::DeviceSize MegaBufferSize{100 * 1024 * 1024}; //!< Size in bytes of the megabuffer (100MiB)

    BufferManager::MegaBufferSlot::MegaBufferSlot(GPU &gpu) : backing(gpu.memory.AllocateBuffer(MegaBufferSize)) {}

    MegaBuffer::MegaBuffer(BufferManager::MegaBufferSlot &slot) : slot{&slot}, freeRegion{slot.backing.subspan(PAGE_SIZE)} {}

    MegaBuffer::~MegaBuffer() {
        if (slot)
            slot->active.clear(std::memory_order_release);
    }

    MegaBuffer &MegaBuffer::operator=(MegaBuffer &&other) {
        if (slot)
            slot->active.clear(std::memory_order_release);
        slot = other.slot;
        freeRegion = other.freeRegion;
        other.slot = nullptr;
        return *this;
    }

    void MegaBuffer::Reset() {
        freeRegion = slot->backing.subspan(PAGE_SIZE);
    }

    vk::Buffer MegaBuffer::GetBacking() const {
        return slot->backing.vkBuffer;
    }

    vk::DeviceSize MegaBuffer::Push(span<u8> data, bool pageAlign) {
        if (data.size() > freeRegion.size())
            throw exception("Ran out of megabuffer space! Alloc size: 0x{:X}", data.size());

        if (pageAlign) {
            // If page aligned data was requested then align the free
            auto alignedFreeBase{util::AlignUp(static_cast<size_t>(freeRegion.data() - slot->backing.data()), PAGE_SIZE)};
            freeRegion = slot->backing.subspan(alignedFreeBase);
        }

        // Allocate space for data from the free region
        auto resultSpan{freeRegion.subspan(0, data.size())};
        resultSpan.copy_from(data);

        // Move the free region along
        freeRegion = freeRegion.subspan(data.size());
        return static_cast<vk::DeviceSize>(resultSpan.data() - slot->backing.data());
    }

    MegaBuffer BufferManager::AcquireMegaBuffer(const std::shared_ptr<FenceCycle> &cycle) {
        std::scoped_lock lock{megaBufferMutex};

        for (auto &slot : megaBuffers) {
            if (!slot.active.test_and_set(std::memory_order_acq_rel)) {
                if (slot.cycle->Poll()) {
                    slot.cycle = cycle;
                    return {slot};
                } else {
                    slot.active.clear(std::memory_order_release);
                }
            }
        }

        auto &megaBuffer{megaBuffers.emplace_back(gpu)};
        megaBuffer.cycle = cycle;
        return {megaBuffer};
    }
}
