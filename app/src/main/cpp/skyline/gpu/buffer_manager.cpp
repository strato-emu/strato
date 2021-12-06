// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>

#include "buffer_manager.h"

namespace skyline::gpu {
    BufferManager::BufferManager(GPU &gpu) : gpu(gpu) {}

    std::shared_ptr<BufferView> BufferManager::FindOrCreate(const GuestBuffer &guest) {
        auto guestMapping{guest.mappings.front()};

        /*
         * Iterate over all buffers that overlap with the first mapping of the guest buffer and compare the mappings:
         * 1) All mappings match up perfectly, we check that the rest of the supplied mappings correspond to mappings in the buffer
         * 1.1) If they match as well, we return a view encompassing the entire buffer
         * 2) Only a contiguous range of mappings match, we check for the overlap bounds, it can go two ways:
         * 2.1) If the supplied buffer is smaller than the matching buffer, we return a view encompassing the mappings into the buffer
         * 2.2) If the matching buffer is smaller than the supplied buffer, we make the matching buffer larger and return it
         * 3) If there's another overlap we go back to (1) with it else we go to (4)
         * 4) Create a new buffer and insert it in the map then return it
         */

        std::scoped_lock lock(mutex);
        std::shared_ptr<Buffer> match{};
        auto mappingEnd{std::upper_bound(buffers.begin(), buffers.end(), guestMapping)}, hostMapping{mappingEnd};
        if (hostMapping != buffers.begin() && (--hostMapping)->end() > guestMapping.begin()) {
            auto &hostMappings{hostMapping->buffer->guest.mappings};
            if (hostMapping->contains(guestMapping)) {
                // We need to check that all corresponding mappings in the candidate buffer and the guest buffer match up
                // Only the start of the first matched mapping and the end of the last mapping can not match up as this is the case for views
                auto firstHostMapping{hostMapping->iterator};
                auto lastGuestMapping{guest.mappings.back()};
                auto endHostMapping{std::find_if(firstHostMapping, hostMappings.end(), [&lastGuestMapping](const span<u8> &it) {
                    return lastGuestMapping.begin() > it.begin() && lastGuestMapping.end() > it.end();
                })}; //!< A past-the-end iterator for the last host mapping, the final valid mapping is prior to this iterator
                bool mappingMatch{std::equal(firstHostMapping, endHostMapping, guest.mappings.begin(), guest.mappings.end(), [](const span<u8> &lhs, const span<u8> &rhs) {
                    return lhs.end() == rhs.end(); // We check end() here to implicitly ignore any offset from the first mapping
                })};

                auto &lastHostMapping{*std::prev(endHostMapping)};
                if (firstHostMapping == hostMappings.begin() && firstHostMapping->begin() == guestMapping.begin() && mappingMatch && endHostMapping == hostMappings.end() && lastGuestMapping.end() == lastHostMapping.end()) {
                    // We've gotten a perfect 1:1 match for *all* mappings from the start to end
                    std::scoped_lock bufferLock(*hostMapping->buffer);
                    return hostMapping->buffer->GetView(0, hostMapping->buffer->size);
                } else if (mappingMatch && firstHostMapping->begin() > guestMapping.begin() && lastHostMapping.end() > lastGuestMapping.end()) {
                    // We've gotten a guest buffer that is located entirely within a host buffer
                    std::scoped_lock bufferLock(*hostMapping->buffer);
                    return hostMapping->buffer->GetView(hostMapping->offset + static_cast<vk::DeviceSize>(hostMapping->begin() - guestMapping.begin()), guest.BufferSize());
                }
            }
        }

        /* TODO: Handle overlapping buffers
        // Create a list of all overlapping buffers and update the guest mappings to fit them all
        boost::container::small_vector<std::pair<std::shared_ptr<Buffer>, u32>, 4> overlappingBuffers;
        GuestBuffer::Mappings newMappings;

        auto guestMappingIt{guest.mappings.begin()};
        while (true) {
            do {
                hostMapping->begin();
                overlappingBuffers.emplace_back(hostMapping->buffer, 4);
            } while (hostMapping != buffers.begin() && (--hostMapping)->end() > guestMappingIt->begin());

            // Iterate over all guest mappings to find overlapping buffers, not just the first
            auto nextGuestMappingIt{std::next(guestMappingIt)};
            if (nextGuestMappingIt != guest.mappings.end())
                hostMapping = std::upper_bound(buffers.begin(), buffers.end(), *nextGuestMappingIt);
            else
                break;
            guestMappingIt = nextGuestMappingIt;
        }

        // Create a buffer that can contain all the overlapping buffers
        auto buffer{std::make_shared<Buffer>(gpu, guest)};

        // Delete mappings from all overlapping buffers and repoint all buffer views
        for (auto &overlappingBuffer : overlappingBuffers) {
            std::scoped_lock overlappingBufferLock(*overlappingBuffer.first);
            auto &bufferMappings{hostMapping->buffer->guest.mappings};

            // Delete all mappings of the overlapping buffers
            while ((++it) != buffer->guest.mappings.end()) {
                guestMapping = *it;
                auto mapping{std::upper_bound(buffers.begin(), buffers.end(), guestMapping)};
                buffers.emplace(mapping, BufferMapping{buffer, it, offset, guestMapping});
                offset += mapping->size_bytes();
            }
        }
         */

        auto buffer{std::make_shared<Buffer>(gpu, guest)};
        auto it{buffer->guest.mappings.begin()};
        buffers.emplace(mappingEnd, BufferMapping{buffer, it, 0, guestMapping});

        vk::DeviceSize offset{};
        while ((++it) != buffer->guest.mappings.end()) {
            guestMapping = *it;
            auto mapping{std::upper_bound(buffers.begin(), buffers.end(), guestMapping)};
            buffers.emplace(mapping, BufferMapping{buffer, it, offset, guestMapping});
            offset += mapping->size_bytes();
        }

        return buffer->GetView(0, buffer->size);
    }
}
