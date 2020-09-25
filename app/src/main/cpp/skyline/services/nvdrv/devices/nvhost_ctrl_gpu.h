// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "nvdevice.h"

namespace skyline::service::nvdrv::device {
    /**
     * @brief NvHostCtrlGpu (/dev/nvhost-ctrl-gpu) is used for context independent operations on the underlying GPU (https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-ctrl-gpu)
     */
    class NvHostCtrlGpu : public NvDevice {
      private:
        std::shared_ptr<type::KEvent> errorNotifierEvent;
        std::shared_ptr<type::KEvent> unknownEvent;

      public:
        NvHostCtrlGpu(const DeviceState &state);

        /**
         * @brief This returns a u32 GPU ZCULL Context Size (https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_ZCULL_GET_CTX_SIZE)
         */
        NvStatus ZCullGetCtxSize(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief This returns a the GPU ZCULL Information (https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_ZCULL_GET_INFO)
         */
        NvStatus ZCullGetInfo(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief This returns a struct with certain GPU characteristics (https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_GET_CHARACTERISTICS)
         */
        NvStatus GetCharacteristics(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief This returns the TPC mask value for each GPC (https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_GET_TPC_MASKS)
         */
        NvStatus GetTpcMasks(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief This returns the mask value for a ZBC slot (https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_ZBC_GET_ACTIVE_SLOT_MASK)
         */
        NvStatus GetActiveSlotMask(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        std::shared_ptr<type::KEvent> QueryEvent(u32 eventId);

        NVDEVICE_DECL(
            NVFUNC(0x4701, NvHostCtrlGpu, ZCullGetCtxSize),
            NVFUNC(0x4702, NvHostCtrlGpu, ZCullGetInfo),
            NVFUNC(0x4706, NvHostCtrlGpu, GetTpcMasks),
            NVFUNC(0x4705, NvHostCtrlGpu, GetCharacteristics),
            NVFUNC(0x4714, NvHostCtrlGpu, GetActiveSlotMask)
        )
    };
}
