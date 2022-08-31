// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/address_space.inc>
#include <soc.h>
#include "nvmap.h"

namespace skyline {
    template class FlatAddressSpaceMap<u32, 0, bool, false, false, 32>;
    template class FlatAllocator<u32, 0, 32>;
}

namespace skyline::service::nvdrv::core {
    NvMap::Handle::Handle(u64 size, Id id) : size(size), alignedSize(size), origSize(size), id(id) {}

    PosixResult NvMap::Handle::Alloc(Flags pFlags, u32 pAlign, u8 pKind, u64 pAddress) {
        std::scoped_lock lock(mutex);

        // Handles cannot be allocated twice
        if (allocated) [[unlikely]]
            return PosixResult::NotPermitted;

        flags = pFlags;
        kind = pKind;
        align = pAlign < constant::PageSize ? constant::PageSize : pAlign;

        // This flag is only applicable for handles with an address passed
        if (pAddress)
            flags.keepUncachedAfterFree = false;
        else
            throw exception("Mapping nvmap handles without a CPU side address is unimplemented!");

        size = util::AlignUp(size, constant::PageSize);
        alignedSize = util::AlignUp(size, align);
        address = pAddress;

        allocated = true;

        return PosixResult::Success;
    }

    PosixResult NvMap::Handle::Duplicate(bool internalSession) {
        std::scoped_lock lock(mutex);

        // Unallocated handles cannot be duplicated as duplication requires memory accounting (in HOS)
        if (!allocated) [[unlikely]]
            return PosixResult::InvalidArgument;

        // If we internally use FromId the duplication tracking of handles won't work accurately due to us not implementing
        // per-process handle refs.
        if (internalSession)
            internalDupes++;
        else
            dupes++;

        return PosixResult::Success;
    }

    NvMap::NvMap(const DeviceState &state) : state(state), smmuAllocator(soc::SmmuPageSize) {}

    void NvMap::AddHandle(std::shared_ptr<Handle> handleDesc) {
        std::scoped_lock lock(handlesLock);

        handles.emplace(handleDesc->id, std::move(handleDesc));
    }

    void NvMap::UnmapHandle(Handle &handleDesc) {
        // Remove pending unmap queue entry if needed
        if (handleDesc.unmapQueueEntry) {
            unmapQueue.erase(*handleDesc.unmapQueueEntry);
            handleDesc.unmapQueueEntry.reset();
        }

        // Free and unmap the handle from the SMMU
        state.soc->smmu.Unmap(handleDesc.pinVirtAddress, static_cast<u32>(handleDesc.alignedSize));
        smmuAllocator.Free(handleDesc.pinVirtAddress, static_cast<u32>(handleDesc.alignedSize));
        handleDesc.pinVirtAddress = 0;
    }


    bool NvMap::TryRemoveHandle(const Handle &handleDesc) {
        // No dupes left, we can remove from handle map
        if (handleDesc.dupes == 0 && handleDesc.internalDupes == 0) {
            std::scoped_lock lock(handlesLock);

            auto it{handles.find(handleDesc.id)};
            if (it != handles.end())
                handles.erase(it);

            return true;
        } else {
            return false;
        }
    }

    PosixResultValue<std::shared_ptr<NvMap::Handle>> NvMap::CreateHandle(u64 size) {
        if (!size) [[unlikely]]
            return PosixResult::InvalidArgument;

        u32 id{nextHandleId.fetch_add(HandleIdIncrement, std::memory_order_relaxed)};
        auto handleDesc{std::make_shared<Handle>(size, id)};
        AddHandle(handleDesc);

        return handleDesc;
    }

    std::shared_ptr<NvMap::Handle> NvMap::GetHandle(Handle::Id handle) {
        std::scoped_lock lock(handlesLock);

        try {
            return handles.at(handle);
        } catch (std::out_of_range &e) {
            return nullptr;
        }
    }

    u32 NvMap::PinHandle(NvMap::Handle::Id handle) {
        auto handleDesc{GetHandle(handle)};
        if (!handleDesc) [[unlikely]]
            return 0;

        std::scoped_lock lock(handleDesc->mutex);
        if (!handleDesc->pins) {
            // If we're in the unmap queue we can just remove ourselves and return since we're already mapped
            {
                // Lock now to prevent our queue entry from being removed for allocation in-between the following check and erase
                std::scoped_lock queueLock(unmapQueueLock);
                if (handleDesc->unmapQueueEntry) {
                    unmapQueue.erase(*handleDesc->unmapQueueEntry);
                    handleDesc->unmapQueueEntry.reset();

                    handleDesc->pins++;
                    return handleDesc->pinVirtAddress;
                }
            }

            // If not then allocate some space and map it
            u32 address{};
            while (!(address = smmuAllocator.Allocate(static_cast<u32>(handleDesc->alignedSize)))) {
                // Free handles until the allocation succeeds
                std::scoped_lock queueLock(unmapQueueLock);
                if (auto freeHandleDesc{unmapQueue.front()}) {
                    // Handles in the unmap queue are guaranteed not to be pinned so don't bother checking if they are before unmapping
                    std::scoped_lock freeLock(freeHandleDesc->mutex);
                    if (handleDesc->pinVirtAddress)
                        UnmapHandle(*freeHandleDesc);
                } else {
                    throw exception("Ran out of SMMU address space!");
                }
            }

            state.soc->smmu.Map(address, reinterpret_cast<u8 *>(handleDesc->address), static_cast<u32>(handleDesc->alignedSize));
            handleDesc->pinVirtAddress = address;
        }

        handleDesc->pins++;
        return handleDesc->pinVirtAddress;
    }

    void NvMap::UnpinHandle(Handle::Id handle) {
        auto handleDesc{GetHandle(handle)};
        if (!handleDesc)
            return;

        std::scoped_lock lock(handleDesc->mutex);
        if (--handleDesc->pins < 0) {
            Logger::Warn("Pin count imbalance detected!");
        } else if (!handleDesc->pins) {
            std::scoped_lock queueLock(unmapQueueLock);

            // Add to the unmap queue allowing this handle's memory to be freed if needed
            unmapQueue.push_back(handleDesc);
            handleDesc->unmapQueueEntry = std::prev(unmapQueue.end());
        }
    }

    std::optional<NvMap::FreeInfo> NvMap::FreeHandle(Handle::Id handle, bool internalSession) {
        std::weak_ptr<Handle> hWeak{GetHandle(handle)};
        FreeInfo freeInfo;

        // We use a weak ptr here so we can tell when the handle has been freed and report that back to guest
        if (auto handleDesc = hWeak.lock()) {
            std::scoped_lock lock(handleDesc->mutex);

            if (internalSession) {
                if (--handleDesc->internalDupes < 0)
                    Logger::Warn("Internal duplicate count imbalance detected!");
            } else {
                if (--handleDesc->dupes < 0) {
                    Logger::Warn("User duplicate count imbalance detected!");
                } else if (handleDesc->dupes == 0) {
                    // Force unmap the handle
                    if (handleDesc->pinVirtAddress) {
                        std::scoped_lock queueLock(unmapQueueLock);
                        UnmapHandle(*handleDesc);
                    }

                    handleDesc->pins = 0;
                }
            }

            // Try to remove the shared ptr to the handle from the map, if nothing else is using the handle
            // then it will now be freed when `h` goes out of scope
            if (TryRemoveHandle(*handleDesc))
                Logger::Debug("Removed nvmap handle: {}", handle);
            else
                Logger::Debug("Tried to free nvmap handle: {} but didn't as it still has duplicates", handle);

            freeInfo = {
                .address = handleDesc->address,
                .size = handleDesc->size,
                .wasUncached = handleDesc->flags.mapUncached,
            };
        } else {
            return std::nullopt;
        }

        // Handle hasn't been freed from memory, set address to 0 to mark that the handle wasn't freed
        if (!hWeak.expired()) {
            Logger::Debug("nvmap handle: {} wasn't freed as it is still in use", handle);
            freeInfo.address = 0;
        }

        return freeInfo;
    }
}
