#pragma once

#include "nvdevice.h"

namespace skyline::gpu::device {
    /**
     * @brief NvHostChannel is used as a common interface for all Channel devices (https://switchbrew.org/wiki/NV_services#Channels)
     */
    class NvHostChannel : public NvDevice {
      private:
        enum class NvChannelPriority : u32 {
            Low = 0x32,
            Medium = 0x64,
            High = 0x94
        };

        u32 timeslice{};

      public:
        NvHostChannel(const DeviceState &state, NvDeviceType type);

        /**
         * @brief This sets the nvmap file descriptor (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SET_NVMAP_FD)
         */
        void SetNvmapFd(IoctlBuffers &buffer);

        /**
         * @brief This allocates a graphic context object (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_ALLOC_OBJ_CTX)
         */
        void AllocObjCtx(IoctlBuffers &buffer);

        /**
         * @brief This initializes the error notifier for this channel (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_ZCULL_BIND)
         */
        void ZcullBind(IoctlBuffers &buffer);

        /**
         * @brief This initializes the error notifier for this channel (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SET_ERROR_NOTIFIER)
         */
        void SetErrorNotifier(IoctlBuffers &buffer);

        /**
         * @brief This sets the priority of the channel (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SET_PRIORITY)
         */
        void SetPriority(IoctlBuffers &buffer);

        /**
         * @brief This allocates a GPFIFO entry (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_ALLOC_GPFIFO_EX2)
         */
        void AllocGpfifoEx2(IoctlBuffers &buffer);

        /**
         * @brief This sets the user specific data (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SET_USER_DATA)
         */
        void SetUserData(IoctlBuffers &buffer);
    };
}
