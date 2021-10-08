// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "nvdevice.h"

namespace skyline::service::nvdrv::device {
    /**
     * @brief NvMap (/dev/nvmap) is used to keep track of buffers and map them onto the SMMU (https://switchbrew.org/wiki/NV_services)
     * @url https://android.googlesource.com/kernel/tegra/+/refs/heads/android-tegra-flounder-3.10-marshmallow/include/linux/nvmap.h
     */
    class NvMap : public NvDevice {
      public:
        using NvMapCore = core::NvMap;

        enum class HandleParameterType : u32 {
            Size = 1,
            Alignment = 2,
            Base = 3,
            Heap = 4,
            Kind = 5,
            IsSharedMemMapped = 6
        };

        NvMap(const DeviceState &state, Driver &driver, Core &core, const SessionContext &ctx);

        /**
         * @brief Creates an nvmap handle for the given size
         * @url https://switchbrew.org/wiki/NV_services#NVMAP_IOC_CREATE
         */
        PosixResult Create(In<u32> size, Out<NvMapCore::Handle::Id> handle);

        /**
         * @brief Creates a new ref to the handle of the given ID
         * @url https://switchbrew.org/wiki/NV_services#NVMAP_IOC_FROM_ID
         */
        PosixResult FromId(In<NvMapCore::Handle::Id> id, Out<NvMapCore::Handle::Id> handle);

        /**
         * @brief Adds the given backing memory to the nvmap handle
         * @url https://switchbrew.org/wiki/NV_services#NVMAP_IOC_ALLOC
         */
        PosixResult Alloc(In<NvMapCore::Handle::Id> handle, In<u32> heapMask, In<NvMapCore::Handle::Flags> flags, InOut<u32> align, In<u8> kind, In<u64> address);

        /**
         * @brief Attempts to free a handle and unpin it from SMMU memory
         * @url https://switchbrew.org/wiki/NV_services#NVMAP_IOC_FREE
         */
        PosixResult Free(In<NvMapCore::Handle::Id> handle, Out<u64> address, Out<u32> size, Out<NvMapCore::Handle::Flags> flags);

        /**
         * @brief Returns info about a property of the nvmap handle
         * @url https://switchbrew.org/wiki/NV_services#NVMAP_IOC_PARAM
         */
        PosixResult Param(In<NvMapCore::Handle::Id> handle, In<HandleParameterType> param, Out<u32> result);

        /**
         * @brief Returns a global ID for the given nvmap handle
         * @url https://switchbrew.org/wiki/NV_services#NVMAP_IOC_GET_ID
         */
        PosixResult GetId(Out<NvMapCore::Handle::Id> id, In<NvMapCore::Handle::Id> handle);

        PosixResult Ioctl(IoctlDescriptor cmd, span<u8> buffer) override;
    };
}
