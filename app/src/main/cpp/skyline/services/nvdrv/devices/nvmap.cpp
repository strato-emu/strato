// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/nvdrv/devices/deserialisation/deserialisation.h>
#include <services/nvdrv/core/nvmap.h>
#include "nvmap.h"

namespace skyline::service::nvdrv::device {
    NvMap::NvMap(const DeviceState &state, Driver &driver, Core &core, const SessionContext &ctx) : NvDevice(state, driver, core, ctx) {}

    PosixResult NvMap::Create(In<u32> size, Out<NvMapCore::Handle::Id> handle) {
        auto handleDesc{core.nvMap.CreateHandle(util::AlignUp(static_cast<u32>(size), constant::PageSize))};
        if (handleDesc) {
            (*handleDesc)->origSize = size; // Orig size is the unaligned size
            handle = (*handleDesc)->id;
            Logger::Debug("handle: {}, size: 0x{:X}", (*handleDesc)->id, size);
        }

        return handleDesc;
    }

    PosixResult NvMap::FromId(In<NvMapCore::Handle::Id> id, Out<NvMapCore::Handle::Id> handle) {
        Logger::Debug("id: {}", id);

        // Handles and IDs are always the same value in nvmap however IDs can be used globally given the right permissions.
        // Since we don't plan on ever supporting multiprocess we can skip implementing handle refs and so this function just does simple validation and passes through the handle id.
        if (!id) [[unlikely]]
            return PosixResult::InvalidArgument;

        auto handleDesc{core.nvMap.GetHandle(id)};
        if (!handleDesc) [[unlikely]]
            return PosixResult::InvalidArgument;

        auto result{handleDesc->Duplicate(ctx.internalSession)};
        if (result == PosixResult::Success)
            handle = id;

        return result;
    }

    PosixResult NvMap::Alloc(In<NvMapCore::Handle::Id> handle,
                             In<u32> heapMask, In<NvMapCore::Handle::Flags> flags,
                             InOut<u32> align, In<u8> kind, In<u64> address) {
        Logger::Debug("handle: {}, flags: ( mapUncached: {}, keepUncachedAfterFree: {} ), align: 0x{:X}, kind: {}, address: 0x{:X}",
                            handle, flags.mapUncached, flags.keepUncachedAfterFree, align, kind, address);

        if (!handle) [[unlikely]]
            return PosixResult::InvalidArgument;

        if (!std::has_single_bit(align)) [[unlikely]]
            return PosixResult::InvalidArgument;

        // Force page size alignment at a minimum
        if (align < constant::PageSize) [[unlikely]]
            align = constant::PageSize;

        auto handleDesc{core.nvMap.GetHandle(handle)};
        if (!handleDesc) [[unlikely]]
            return PosixResult::InvalidArgument;

        return handleDesc->Alloc(flags, align, kind, address);
    }

    PosixResult NvMap::Free(In<NvMapCore::Handle::Id> handle,
                            Out<u64> address, Out<u32> size,
                            Out<NvMapCore::Handle::Flags> flags) {
        Logger::Debug("handle: {}", handle);

        if (!handle) [[unlikely]]
            return PosixResult::Success;

        if (auto freeInfo{core.nvMap.FreeHandle(handle, ctx.internalSession)}) {
            address = freeInfo->address;
            size = static_cast<u32>(freeInfo->size);
            flags = NvMapCore::Handle::Flags{ .mapUncached = freeInfo->wasUncached };
        } else {
            Logger::Debug("Handle not freed");
        }

        return PosixResult::Success;
    }

    PosixResult NvMap::Param(In<NvMapCore::Handle::Id> handle, In<HandleParameterType> param, Out<u32> result) {
        Logger::Debug("handle: {}, param: {}", handle, param);

        if (!handle)
            return PosixResult::InvalidArgument;

        auto handleDesc{core.nvMap.GetHandle(handle)};
        if (!handleDesc) [[unlikely]]
            return PosixResult::InvalidArgument;

        switch (param) {
            case HandleParameterType::Size:
                result = static_cast<u32>(handleDesc->origSize);
                return PosixResult::Success;
            case HandleParameterType::Alignment:
                result = static_cast<u32>(handleDesc->align);
                return PosixResult::Success;
            case HandleParameterType::Base:
                result = static_cast<u32>(-static_cast<i32>(PosixResult::InvalidArgument));
                return PosixResult::Success;
            case HandleParameterType::Heap:
                if (handleDesc->allocated)
                    result = 0x40000000;
                else
                    result = 0;

                return PosixResult::Success;
            case HandleParameterType::Kind:
                result = handleDesc->kind;
                return PosixResult::Success;
            case HandleParameterType::IsSharedMemMapped:
                result = handleDesc->isSharedMemMapped;
                return PosixResult::Success;
            default:
                return PosixResult::InvalidArgument;
        }
    }

    PosixResult NvMap::GetId(Out<NvMapCore::Handle::Id> id, In<NvMapCore::Handle::Id> handle) {
        Logger::Debug("handle: {}", handle);

        // See the comment in FromId for extra info on this function
        if (!handle) [[unlikely]]
            return PosixResult::InvalidArgument;

        auto handleDesc{core.nvMap.GetHandle(handle)};
        if (!handleDesc) [[unlikely]]
            return PosixResult::NotPermitted; // This will always return EPERM irrespective of if the handle exists or not

        id = handleDesc->id;
        return PosixResult::Success;
    }

#include "deserialisation/macro_def.inc"
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
#include "deserialisation/macro_undef.inc"
}

