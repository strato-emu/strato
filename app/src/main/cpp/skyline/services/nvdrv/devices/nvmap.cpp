// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/nvdrv/devices/deserialisation/deserialisation.h>
#include <services/nvdrv/core/nvmap.h>
#include "nvmap.h"

namespace skyline::service::nvdrv::device {
    NvMap::NvMap(const DeviceState &state, Core &core, const SessionContext &ctx) : NvDevice(state, core, ctx) {}

    PosixResult NvMap::Create(In<u32> size, Out<NvMapCore::Handle::Id> handle) {
        auto h{core.nvMap.CreateHandle(util::AlignUp(size, PAGE_SIZE))};
        if (h) {
            (*h)->origSize = size; // Orig size is the unaligned size
            handle = (*h)->id;
            state.logger->Debug("handle: {}, size: 0x{:X}", (*h)->id, size);
        }

        return h;
    }

    PosixResult NvMap::FromId(In<NvMapCore::Handle::Id> id, Out<NvMapCore::Handle::Id> handle) {
        // Handles and IDs are always the same value in nvmap however IDs can be used globally given the right permissions.
        // Since we don't plan on ever supporting multiprocess we can skip implementing handle refs and so this function
        // just does simple validation and passes through the handle id.
        if (!id) [[unlikely]]
            return PosixResult::InvalidArgument;

        auto h{core.nvMap.GetHandle(id)};
        if (!h) [[unlikely]]
            return PosixResult::InvalidArgument;

        return h->Duplicate(ctx.internalSession);
    }

    PosixResult NvMap::Alloc(In<NvMapCore::Handle::Id> handle, In<u32> heapMask, In<NvMapCore::Handle::Flags> flags, InOut<u32> align, In<u8> kind, In<u64> address) {
        if (!handle) [[unlikely]]
            return PosixResult::InvalidArgument;

        if (!std::ispow2(align)) [[unlikely]]
            return PosixResult::InvalidArgument;

        // Force page size alignment at a minimum
        if (align < PAGE_SIZE) [[unlikely]]
            align = PAGE_SIZE;

        auto h{core.nvMap.GetHandle(handle)};
        if (!h) [[unlikely]]
            return PosixResult::InvalidArgument;

        state.logger->Debug("handle: {}, flags: ( mapUncached: {}, keepUncachedAfterFree: {} ), align: 0x{:X}, kind: {}, address: 0x{:X}", handle, flags.mapUncached, flags.keepUncachedAfterFree, align, kind, address);

        return h->Alloc(flags, align, kind, address);
    }

    PosixResult NvMap::Free(In<NvMapCore::Handle::Id> handle, Out<u64> address, Out<u32> size, Out<NvMapCore::Handle::Flags> flags) {
        if (!handle) [[unlikely]]
            return PosixResult::Success;

        if (auto freeInfo{core.nvMap.FreeHandle(handle, ctx.internalSession)}) {
            address = freeInfo->address;
            size = static_cast<u32>(freeInfo->size);
            flags = NvMapCore::Handle::Flags{ .mapUncached = freeInfo->wasUncached };
        }

        return PosixResult::Success;
    }

    PosixResult NvMap::Param(In<NvMapCore::Handle::Id> handle, In<HandleParameterType> param, Out<u32> result) {
        if (!handle)
            return PosixResult::InvalidArgument;

        auto h{core.nvMap.GetHandle(handle)};
        if (!h) [[unlikely]]
            return PosixResult::InvalidArgument;

        switch (param) {
            case HandleParameterType::Size:
                result = h->origSize;
                return PosixResult::Success;
            case HandleParameterType::Alignment:
                result = h->align;
                return PosixResult::Success;
            case HandleParameterType::Base:
                result = -static_cast<i32>(PosixResult::InvalidArgument);
                return PosixResult::Success;
            case HandleParameterType::Heap:
                if (h->allocated)
                    result = 0x40000000;
                else
                    result = 0;

                return PosixResult::Success;
            case HandleParameterType::Kind:
                result = h->kind;
                return PosixResult::Success;
            case HandleParameterType::IsSharedMemMapped:
                result = h->isSharedMemMapped;
                return PosixResult::Success;
            default:
                return PosixResult::InvalidArgument;
        }
    }

    PosixResult NvMap::GetId(Out<NvMapCore::Handle::Id> id, In<NvMapCore::Handle::Id> handle) {
        // See the comment in FromId for extra info on this function
        if (!handle) [[unlikely]]
            return PosixResult::InvalidArgument;

        auto h{core.nvMap.GetHandle(handle)};
        if (!h) [[unlikely]]
            return PosixResult::NotPermitted; // This will always return EPERM irrespective of if the handle exists or not

        id = h->id;
        return PosixResult::Success;
    }

#include "deserialisation/macro_def.h"
    static constexpr u32 NvMapMagic{1};

    IOCTL_HANDLER_FUNC(NvMap, ({
        IOCTL_CASE_ARGS(INOUT, SIZE(0x8),  MAGIC(NvMapMagic), FUNC(0x1),
                        Create, ARGS(In<u32>, Out<NvMapCore::Handle::Id>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x8),  MAGIC(NvMapMagic), FUNC(0x3),
                        FromId, ARGS(In<NvMapCore::Handle::Id>, Out<NvMapCore::Handle::Id>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x20), MAGIC(NvMapMagic), FUNC(0x4),
                        Alloc,  ARGS(In<NvMapCore::Handle::Id>, In<u32>, In<NvMapCore::Handle::Flags>, InOut<u32>, In<u8>, Pad<u8, 0x7>, In<u64>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x18), MAGIC(NvMapMagic), FUNC(0x5),
                        Free,   ARGS(In<NvMapCore::Handle::Id>, Pad<u32>, Out<u64>, Out<u32>, Out<NvMapCore::Handle::Flags>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0xC),  MAGIC(NvMapMagic), FUNC(0x9),
                        Param,  ARGS(In<NvMapCore::Handle::Id>, In<HandleParameterType>, Out<u32>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x8),  MAGIC(NvMapMagic), FUNC(0xE),
                        GetId,  ARGS(Out<NvMapCore::Handle::Id>, In<NvMapCore::Handle::Id>))
    }))
#include "deserialisation/macro_undef.h"
}

