// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nvmap.h"

namespace skyline::service::nvdrv::core {
    NvMap::Handle::Handle(u64 size, Id id) : size(size), alignedSize(size), origSize(size), id(id) {}

    PosixResult NvMap::Handle::Alloc(Flags pFlags, u32 pAlign, u8 pKind, u64 pAddress) {
        std::scoped_lock lock(mutex);

        // Handles cannot be allocated twice
        if (allocated) [[unlikely]]
            return PosixResult::NotPermitted;

        flags = pFlags;
        kind = pKind;
        align = pAlign < PAGE_SIZE ? PAGE_SIZE : pAlign;

        // This flag is only applicable for handles with an address passed
        if (pAddress)
            flags.keepUncachedAfterFree = false;
        else
            throw exception("Mapping nvmap handles without a CPU side address is unimplemented!");

        size = util::AlignUp(size, PAGE_SIZE);
        alignedSize = util::AlignUp(size, align);
        address = pAddress;

        // TODO: pin init

        allocated = true;

        return PosixResult::Success;
    }

    PosixResult NvMap::Handle::Duplicate(bool internalSession) {
        // Unallocated handles cannot be duplicated as duplication requires memory accounting (in HOS)
        if (!allocated) [[unlikely]]
            return PosixResult::InvalidArgument;

        std::scoped_lock lock(mutex);

        // If we internally use FromId the duplication tracking of handles won't work accurately due to us not implementing
        // per-process handle refs.
        if (internalSession)
            internalDupes++;
        else
            dupes++;

        return PosixResult::Success;
    }

    NvMap::NvMap(const DeviceState &state) : state(state) {}

    void NvMap::AddHandle(std::shared_ptr<Handle> handleDesc) {
        std::scoped_lock lock(handlesLock);

        handles.emplace(handleDesc->id, std::move(handleDesc));
    }

    bool NvMap::TryRemoveHandle(const std::shared_ptr<Handle> &handleDesc) {
        // No dupes left, we can remove from handle map
        if (handleDesc->dupes == 0 && handleDesc->internalDupes == 0) {
            std::scoped_lock lock(handlesLock);

            auto it{handles.find(handleDesc->id)};
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

    std::optional<NvMap::FreeInfo> NvMap::FreeHandle(Handle::Id handle, bool internalSession) {
        std::weak_ptr<Handle> hWeak{GetHandle(handle)};
        FreeInfo freeInfo;

        // We use a weak ptr here so we can tell when the handle has been freed and report that back to guest
        if (auto handleDesc = hWeak.lock()) {
            if (!handleDesc) [[unlikely]]
                return std::nullopt;

            std::scoped_lock lock(handleDesc->mutex);

            if (internalSession) {
                if (--handleDesc->internalDupes < 0)
                    state.logger->Warn("Internal duplicate count inbalance detected!");
            } else {
                if (--handleDesc->dupes < 0) {
                    state.logger->Warn("User duplicate count inbalance detected!");
                } else if (handleDesc->dupes == 0) {
                    // TODO: unpin
                }
            }

            // Try to remove the shared ptr to the handle from the map, if nothing else is using the handle
            // then it will now be freed when `h` goes out of scope
            if (TryRemoveHandle(handleDesc))
                state.logger->Debug("Removed nvmap handle: {}", handle);
            else
                state.logger->Debug("Tried to free nvmap handle: {} but didn't as it still has duplicates", handle);

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
            state.logger->Debug("nvmap handle: {} wasn't freed as it is still in use", handle);
            freeInfo.address = 0;
        }

        return freeInfo;
    }
}