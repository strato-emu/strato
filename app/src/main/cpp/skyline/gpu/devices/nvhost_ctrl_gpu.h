#pragma once

#include "nvdevice.h"

namespace skyline::gpu::device {
    /**
     * @brief NvHostCtrlGpu (/dev/nvhost-ctrl-gpu) is used for context independent operations on the underlying GPU (https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-ctrl-gpu)
     */
    class NvHostCtrlGpu : public NvDevice {
      public:
        NvHostCtrlGpu(const DeviceState &state);

        /**
         * @brief This returns a u32 GPU ZCULL Context Size (https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_ZCULL_GET_CTX_SIZE)
         */
        void ZCullGetCtxSize(IoctlData &buffer);

        /**
         * @brief This returns a the GPU ZCULL Information (https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_ZCULL_GET_INFO)
         */
        void ZCullGetInfo(IoctlData &buffer);

        /**
         * @brief This returns a struct with certain GPU characteristics (https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_GET_CHARACTERISTICS)
         */
        void GetCharacteristics(IoctlData &buffer);

        /**
         * @brief This returns the TPC mask value for each GPC (https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_GET_TPC_MASKS)
         */
        void GetTpcMasks(IoctlData &buffer);

        /**
         * @brief This returns the mask value for a ZBC slot (https://switchbrew.org/wiki/NV_services#NVGPU_GPU_IOCTL_ZBC_GET_ACTIVE_SLOT_MASK)
         */
        void GetActiveSlotMask(IoctlData &buffer);
    };
}
