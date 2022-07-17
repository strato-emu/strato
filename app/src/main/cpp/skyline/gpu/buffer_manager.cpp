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

    BufferView BufferManager::FindOrCreate(GuestBuffer guestMapping, ContextTag tag, const std::function<void(std::shared_ptr<Buffer>, ContextLock<Buffer> &&)> &attachBuffer) {
        /*
         * We align the buffer to the page boundary to ensure that:
         * 1) Any buffer view has the same alignment guarantees as on the guest, this is required for UBOs, SSBOs and Texel buffers
         * 2) We can coalesce a lot of tiny buffers into a single large buffer covering an entire page, this is often the case for index buffers and vertex buffers
         */
        auto alignedStart{util::AlignDown(guestMapping.begin().base(), PAGE_SIZE)}, alignedEnd{util::AlignUp(guestMapping.end().base(), PAGE_SIZE)};
        vk::DeviceSize offset{static_cast<size_t>(guestMapping.begin().base() - alignedStart)}, size{guestMapping.size()};
        guestMapping = span<u8>{alignedStart, alignedEnd};

        struct LockedBuffer {
            std::shared_ptr<Buffer> buffer;
            ContextLock<Buffer> lock;

            LockedBuffer(std::shared_ptr<Buffer> pBuffer, ContextTag tag) : buffer{std::move(pBuffer)}, lock{tag, *buffer} {}

            Buffer *operator->() const {
                return buffer.get();
            }

            std::shared_ptr<Buffer> &operator*() {
                return buffer;
            }
        };

        // Lookup for any buffers overlapping with the supplied guest mapping
        boost::container::small_vector<LockedBuffer, 4> overlaps;
        for (auto entryIt{std::lower_bound(buffers.begin(), buffers.end(), guestMapping.end().base(), BufferLessThan)}; entryIt != buffers.begin() && (*--entryIt)->guest->begin() <= guestMapping.end();)
            if ((*entryIt)->guest->end() > guestMapping.begin())
                overlaps.emplace_back(*entryIt, tag);

        if (overlaps.size() == 1) [[likely]] {
            auto &buffer{overlaps.front()};
            if (buffer->guest->begin() <= guestMapping.begin() && buffer->guest->end() >= guestMapping.end()) {
                // If we find a buffer which can entirely fit the guest mapping, we can just return a view into it
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

        LockedBuffer newBuffer{std::make_shared<Buffer>(gpu, span<u8>{lowestAddress, highestAddress}), tag}; // If we don't lock the buffer prior to trapping it during synchronization, a race could occur with a guest trap acquiring the lock before we do and mutating the buffer prior to it being ready

        newBuffer->SynchronizeHost(false); // Overlaps don't necessarily fully cover the buffer so we have to perform a sync here to prevent any gaps

        auto copyBuffer{[](auto dstGuest, auto srcGuest, auto dstPtr, auto srcPtr) {
            if (dstGuest.begin().base() <= srcGuest.begin().base()) {
                size_t dstOffset{static_cast<size_t>(srcGuest.begin().base() - dstGuest.begin().base())};
                size_t copySize{std::min(dstGuest.size() - dstOffset, srcGuest.size())};
                std::memcpy(dstPtr + dstOffset, srcPtr, copySize);
            } else if (dstGuest.begin().base() > srcGuest.begin().base()) {
                size_t srcOffset{static_cast<size_t>(dstGuest.begin().base() - srcGuest.begin().base())};
                size_t copySize{std::min(dstGuest.size(), srcGuest.size() - srcOffset)};
                std::memcpy(dstPtr, srcPtr + srcOffset, copySize);
            }
        }}; //!< Copies between two buffers based off of their mappings in guest memory

        bool shouldAttach{}; //!< If the new buffer should be attached to the current context
        for (auto &srcBuffer : overlaps) {
            if (!srcBuffer.lock.IsFirstUsage())
                // If any overlapping buffer was already attached to the current context, we should also attach the current context
                shouldAttach = true;

            // All newly created buffers that have this set are guaranteed to be attached in buffer FindOrCreate, attach will then lock the buffer without resetting this flag, which will only finally be reset when the lock is released
            newBuffer->usedByContext |= srcBuffer->usedByContext;
            newBuffer->everHadInlineUpdate |= srcBuffer->everHadInlineUpdate;

            if (srcBuffer->cycle && newBuffer->cycle != srcBuffer->cycle)
                if (newBuffer->cycle)
                    newBuffer->cycle->ChainCycle(srcBuffer->cycle);
                else
                    newBuffer->cycle = srcBuffer->cycle;

            if (srcBuffer->dirtyState == Buffer::DirtyState::GpuDirty) {
                srcBuffer->WaitOnFence();

                if (srcBuffer.lock.IsFirstUsage() && newBuffer->dirtyState != Buffer::DirtyState::GpuDirty)
                    copyBuffer(*newBuffer->guest, *srcBuffer->guest, newBuffer->mirror.data(), srcBuffer->backing.data());
                else
                    newBuffer->MarkGpuDirty();
            } else if (srcBuffer->usedByContext) {
                if (srcBuffer->dirtyState == Buffer::DirtyState::CpuDirty)
                    Logger::Error("Buffer (0x{}-0x{}) is marked as CPU dirty while being utilized by the context, this is not valid", srcBuffer->guest->begin().base(), srcBuffer->guest->end().base());
                // We need the backing to be stable so that any accesses within this context are sequenced correctly, we can't use the source mirror here either since buffer writes within this context will update the mirror on CPU and backing on GPU
                srcBuffer->WaitOnFence();
                copyBuffer(*newBuffer->guest, *srcBuffer->guest, newBuffer->backing.data(), srcBuffer->backing.data());
            }

            // Transfer all views from the overlapping buffer to the new buffer with the new buffer and updated offset, ensuring pointer stability
            vk::DeviceSize overlapOffset{static_cast<vk::DeviceSize>(srcBuffer->guest->begin() - newBuffer->guest->begin())};
            for (auto it{srcBuffer->views.begin()}; it != srcBuffer->views.end(); it++) {
                if (overlapOffset)
                    // This is a slight hack as we really shouldn't be changing the underlying non-mutable set elements without a rehash but without writing our own set impl this is the best we can do
                    const_cast<Buffer::BufferViewStorage *>(&*it)->offset += overlapOffset;

                // Reset the sequence number to the initial one, if the new buffer was created from any GPU dirty overlaps then the new buffer's sequence will be incremented past this thus forcing a reacquire if necessary
                // This is fine to do in the set since the hash and operator== do not use this value
                it->lastAcquiredSequence = Buffer::InitialSequenceNumber;
            }

            if (overlapOffset)
                // All current hashes are invalidated by above loop if overlapOffset is nonzero so rehash the container
                srcBuffer->views.rehash(0);

            // Merge the view sets, this will keep pointer stability hence avoiding any reallocation
            newBuffer->views.merge(srcBuffer->views);

            // Transfer all delegates references from the overlapping buffer to the new buffer
            for (auto &delegate : srcBuffer->delegates) {
                delegate->buffer = *newBuffer;
                if (delegate->usageCallback)
                    delegate->usageCallback(*delegate->view, *newBuffer);
            }

            newBuffer->delegates.splice(newBuffer->delegates.end(), srcBuffer->delegates);

            srcBuffer->Invalidate(); // Invalidate the overlapping buffer so it can't be synced in the future
            buffers.erase(std::find(buffers.begin(), buffers.end(), srcBuffer.buffer));
        }

        if (shouldAttach)
            attachBuffer(*newBuffer, std::move(newBuffer.lock));

        buffers.insert(std::lower_bound(buffers.begin(), buffers.end(), newBuffer->guest->end().base(), BufferLessThan), *newBuffer);

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

    bool MegaBuffer::WasUsed() {
        return freeRegion != slot->backing.subspan(PAGE_SIZE);
    }

    void MegaBuffer::ReplaceCycle(const std::shared_ptr<FenceCycle> &cycle) {
        slot->cycle = cycle;
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
