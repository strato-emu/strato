// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/common/fence.h>
#include "nvdevice.h"

namespace skyline::service::nvdrv::device {
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

        Fence channelFence{};
        u32 timeslice{};
        std::shared_ptr<type::KEvent> smExceptionBreakpointIntReportEvent;
        std::shared_ptr<type::KEvent> smExceptionBreakpointPauseReportEvent;
        std::shared_ptr<type::KEvent> errorNotifierEvent;

      public:
        NvHostChannel(const DeviceState &state);

        /**
         * @brief This sets the nvmap file descriptor (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SET_NVMAP_FD)
         */
        NvStatus SetNvmapFd(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief This sets the timeout for the channel (https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CHANNEL_SET_SUBMIT_TIMEOUT)
         */
        NvStatus SetSubmitTimeout(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief This submits a command to the GPFIFO (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SUBMIT_GPFIFO)
         */
        NvStatus SubmitGpfifo(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief This allocates a graphic context object (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_ALLOC_OBJ_CTX)
         */
        NvStatus AllocObjCtx(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief This initializes the error notifier for this channel (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_ZCULL_BIND)
         */
        NvStatus ZcullBind(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief This initializes the error notifier for this channel (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SET_ERROR_NOTIFIER)
         */
        NvStatus SetErrorNotifier(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief This sets the priority of the channel (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SET_PRIORITY)
         */
        NvStatus SetPriority(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief This allocates a GPFIFO entry (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_ALLOC_GPFIFO_EX2)
         */
        NvStatus AllocGpfifoEx2(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief This sets the user specific data (https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SET_USER_DATA)
         */
        NvStatus SetUserData(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        std::shared_ptr<type::KEvent> QueryEvent(u32 eventId);

        NVDEVICE_DECL(
            NVFUNC(0x4801, NvHostChannel, SetNvmapFd),
            NVFUNC(0x4803, NvHostChannel, SetSubmitTimeout),
            NVFUNC(0x4808, NvHostChannel, SubmitGpfifo),
            NVFUNC(0x4809, NvHostChannel, AllocObjCtx),
            NVFUNC(0x480B, NvHostChannel, ZcullBind),
            NVFUNC(0x480C, NvHostChannel, SetErrorNotifier),
            NVFUNC(0x480D, NvHostChannel, SetPriority),
            NVFUNC(0x481A, NvHostChannel, AllocGpfifoEx2),
            NVFUNC(0x4714, NvHostChannel, SetUserData)
        )
    };
}
