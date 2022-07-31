// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include "buffer_manager.h"

namespace skyline::gpu {
    BufferManager::BufferManager(GPU &gpu) : gpu{gpu} {}

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

    BufferManager::LockedBuffer::LockedBuffer(std::shared_ptr<Buffer> pBuffer, ContextTag tag) : buffer{std::move(pBuffer)}, lock{tag, *buffer}, stateLock(buffer->stateMutex) {}

    Buffer *BufferManager::LockedBuffer::operator->() const {
        return buffer.get();
    }

    std::shared_ptr<Buffer> &BufferManager::LockedBuffer::operator*() {
        return buffer;
    }

    BufferManager::LockedBuffers BufferManager::Lookup(span<u8> range, ContextTag tag) {
        LockedBuffers overlaps;

        // Try to do a fast lookup in the page table
        auto lookupBuffer{bufferTable[range.begin().base()]};
        if (lookupBuffer != nullptr && lookupBuffer->guest->contains(range)) {
            overlaps.emplace_back(lookupBuffer->shared_from_this(), tag);
            return overlaps;
        }

        // If we cannot find the buffer quickly, do a binary search to find all overlapping buffers
        auto entryIt{std::lower_bound(bufferMappings.begin(), bufferMappings.end(), range.end().base(), BufferLessThan)};
        while (entryIt != bufferMappings.begin() && (*--entryIt)->guest->begin() <= range.end())
            if ((*entryIt)->guest->end() > range.begin())
                overlaps.emplace_back(*entryIt, tag);

        return overlaps;
    }

    void BufferManager::InsertBuffer(std::shared_ptr<Buffer> buffer) {
        auto bufferStart{buffer->guest->begin().base()}, bufferEnd{buffer->guest->end().base()};
        bufferTable.Set(bufferStart, bufferEnd, buffer.get());
        bufferMappings.insert(std::lower_bound(bufferMappings.begin(), bufferMappings.end(), bufferEnd, BufferLessThan), std::move(buffer));
    }

    void BufferManager::DeleteBuffer(const std::shared_ptr<Buffer> &buffer) {
        bufferTable.Set(buffer->guest->begin().base(), buffer->guest->end().base(), nullptr);
        bufferMappings.erase(std::find(bufferMappings.begin(), bufferMappings.end(), buffer));
    }

    BufferManager::LockedBuffer BufferManager::CoalesceBuffers(span<u8> range, const LockedBuffers &srcBuffers, ContextTag tag) {
        if (!range.valid())
            range = span<u8>{srcBuffers.front().buffer->guest->begin(), srcBuffers.back().buffer->guest->end()};

        auto lowestAddress{range.begin().base()}, highestAddress{range.end().base()};
        for (const auto &srcBuffer : srcBuffers) {
            // Find the extents of the new buffer we want to create that can hold all overlapping buffers
            auto mapping{*srcBuffer->guest};
            if (mapping.begin().base() < lowestAddress)
                lowestAddress = mapping.begin().base();
            if (mapping.end().base() > highestAddress)
                highestAddress = mapping.end().base();
        }

        LockedBuffer newBuffer{std::make_shared<Buffer>(gpu, span<u8>{lowestAddress, highestAddress}), tag}; // If we don't lock the buffer prior to trapping it during synchronization, a race could occur with a guest trap acquiring the lock before we do and mutating the buffer prior to it being ready

        newBuffer->SetupGuestMappings();
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

        for (auto &srcBuffer : srcBuffers) {
            // All newly created buffers that have this set are guaranteed to be attached in buffer FindOrCreate, attach will then lock the buffer without resetting this flag, which will only finally be reset when the lock is released
            if (newBuffer->backingImmutability == Buffer::BackingImmutability::None && srcBuffer->backingImmutability != Buffer::BackingImmutability::None)
                newBuffer->backingImmutability = srcBuffer->backingImmutability;
            else if (srcBuffer->backingImmutability == Buffer::BackingImmutability::AllWrites)
                newBuffer->backingImmutability = Buffer::BackingImmutability::AllWrites;

            newBuffer->everHadInlineUpdate |= srcBuffer->everHadInlineUpdate;

            // LockedBuffer also holds `stateMutex` so we can freely access this
            if (srcBuffer->cycle && newBuffer->cycle != srcBuffer->cycle) {
                if (newBuffer->cycle)
                    newBuffer->cycle->ChainCycle(srcBuffer->cycle);
                else
                    newBuffer->cycle = srcBuffer->cycle;
            }

            if (srcBuffer->dirtyState == Buffer::DirtyState::GpuDirty) {
                srcBuffer->WaitOnFence();

                if (srcBuffer.lock.IsFirstUsage() && newBuffer->dirtyState != Buffer::DirtyState::GpuDirty)
                    copyBuffer(*newBuffer->guest, *srcBuffer->guest, newBuffer->mirror.data(), srcBuffer->backing.data());
                else
                    newBuffer->MarkGpuDirty();

                // Since we don't synchost source buffers and the source buffers here are GPU dirty their mirrors will be out of date, meaning the backing contents of this source buffer's region in the new buffer from the initial synchost call will be incorrect. By copying backings directly here we can ensure that no writes are lost and that if the newly created buffer needs to turn GPU dirty during recreation no copies need to be done since the backing is as up to date as the mirror at a minimum.
                copyBuffer(*newBuffer->guest, *srcBuffer->guest, newBuffer->backing.data(), srcBuffer->backing.data());
            } else if (srcBuffer->AllCpuBackingWritesBlocked()) {
                if (srcBuffer->dirtyState == Buffer::DirtyState::CpuDirty)
                    Logger::Error("Buffer (0x{}-0x{}) is marked as CPU dirty while CPU backing writes are blocked, this is not valid", srcBuffer->guest->begin().base(), srcBuffer->guest->end().base());

                // We need the backing to be stable so that any writes within this context are sequenced correctly, we can't use the source mirror here either since buffer writes within this context will update the mirror on CPU and backing on GPU
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
                if (delegate->usageCallbacks)
                    for (auto &callback : *delegate->usageCallbacks)
                        callback(*delegate->view, *newBuffer);
            }

            newBuffer->delegates.splice(newBuffer->delegates.end(), srcBuffer->delegates);
        }

        return newBuffer;
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

        auto overlaps{Lookup(guestMapping, tag)};
        if (overlaps.size() == 1) [[likely]] {
            // If we find a buffer which can entirely fit the guest mapping, we can just return a view into it
            auto &firstOverlap{overlaps.front()};
            if (firstOverlap->guest->begin() <= guestMapping.begin() && firstOverlap->guest->end() >= guestMapping.end())
                return firstOverlap->GetView(static_cast<vk::DeviceSize>(guestMapping.begin() - firstOverlap->guest->begin()) + offset, size);
        }

        if (overlaps.empty()) {
            // If we couldn't find any overlapping buffers, create a new buffer without coalescing
            LockedBuffer buffer{std::make_shared<Buffer>(gpu, guestMapping), tag};
            buffer->SetupGuestMappings();
            InsertBuffer(*buffer);
            return buffer->GetView(offset, size);
        } else {
            // If the new buffer overlaps other buffers, we need to create a new buffer and coalesce all overlapping buffers into one
            auto buffer{CoalesceBuffers(guestMapping, overlaps, tag)};

            // If any overlapping buffer was already attached to the current context, we should also attach the new buffer
            for (auto &srcBuffer : overlaps) {
                if (!srcBuffer.lock.IsFirstUsage()) {
                    attachBuffer(*buffer, std::move(buffer.lock));
                    break;
                }
            }

            // Delete older overlapping buffers and insert the new buffer into the map
            for (auto &overlap : overlaps) {
                DeleteBuffer(*overlap);
                overlap->Invalidate(); // Invalidate the overlapping buffer so it can't be synced in the future
            }
            InsertBuffer(*buffer);

            return buffer->GetView(static_cast<vk::DeviceSize>(guestMapping.begin() - buffer->guest->begin()) + offset, size);
        }
    }
}
