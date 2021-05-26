// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/common/fence.h>
#include "nvdevice.h"

namespace skyline::service::nvdrv::device {
    /**
     * @brief NvHostChannel is used as a common interface for all Channel devices
     * @url https://switchbrew.org/wiki/NV_services#Channels
     */
    class NvHostChannel : public NvDevice {
      private:
        enum class NvChannelPriority : u32 {
            Low = 0x32,
            Medium = 0x64,
            High = 0x94,
        };

        Fence channelFence{};
        u32 timeslice{};
        std::shared_ptr<type::KEvent> smExceptionBreakpointIntReportEvent;
        std::shared_ptr<type::KEvent> smExceptionBreakpointPauseReportEvent;
        std::shared_ptr<type::KEvent> errorNotifierEvent;

      public:
        NvHostChannel(const DeviceState &state);

        /**
         * @brief Sets the nvmap file descriptor
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SET_NVMAP_FD
         */
        NvStatus SetNvmapFd(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief Sets the timeout for the channel
         * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CHANNEL_SET_SUBMIT_TIMEOUT
         */
        NvStatus SetSubmitTimeout(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief Submits a command to the GPFIFO
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SUBMIT_GPFIFO
         */
        NvStatus SubmitGpfifo(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief Allocates a graphic context object
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_ALLOC_OBJ_CTX
         */
        NvStatus AllocObjCtx(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief Initializes the error notifier for this channel
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_ZCULL_BIND
         */
        NvStatus ZcullBind(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief Initializes the error notifier for this channel
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SET_ERROR_NOTIFIER
         */
        NvStatus SetErrorNotifier(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief Sets the priority of the channel
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SET_PRIORITY
         */
        NvStatus SetPriority(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief Allocates a GPFIFO entry
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_ALLOC_GPFIFO_EX2
         */
        NvStatus AllocGpfifoEx2(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief Sets the timeslice of the channel
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SET_TIMESLICE)
         */
        NvStatus SetTimeslice(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief Sets the user specific data
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SET_USER_DATA
         */
        NvStatus SetUserData(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        std::shared_ptr<type::KEvent> QueryEvent(u32 eventId) override;

        NVDEVICE_DECL(
            NVFUNC(0x4801, NvHostChannel, SetNvmapFd),
            NVFUNC(0x4803, NvHostChannel, SetSubmitTimeout),
            NVFUNC(0x4808, NvHostChannel, SubmitGpfifo),
            NVFUNC(0x4809, NvHostChannel, AllocObjCtx),
            NVFUNC(0x480B, NvHostChannel, ZcullBind),
            NVFUNC(0x480C, NvHostChannel, SetErrorNotifier),
            NVFUNC(0x480D, NvHostChannel, SetPriority),
            NVFUNC(0x481A, NvHostChannel, AllocGpfifoEx2),
            NVFUNC(0x481B, NvHostChannel, SubmitGpfifo), // Our SubmitGpfifo implementation also handles SubmitGpfifoEx
            NVFUNC(0x481D, NvHostChannel, SetTimeslice),
            NVFUNC(0x4714, NvHostChannel, SetUserData)
        )
    };
}
