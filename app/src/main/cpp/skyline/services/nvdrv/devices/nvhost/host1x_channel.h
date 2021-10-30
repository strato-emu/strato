// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/common/fence.h>
#include <services/nvdrv/devices/nvdevice.h>
#include <services/nvdrv/core/syncpoint_manager.h>

namespace skyline::service::nvdrv::device::nvhost {
    /**
     * @brief nvhost::Host1XChannel is used by applications to interface with host1x channels, such as VIC and NVDEC
     * @url https://switchbrew.org/wiki/NV_services#Channels
     */
    class Host1XChannel : public NvDevice {
      private:
        core::ChannelType channelType; //!< The specific host1x channel that this instance refers to
        std::mutex channelMutex; //!< Synchronises submit operations

      public:
        /**
         * @brief Describes how a gather for a submit should be generated from a given handle
         */
        struct SubmitCmdBuf {
            core::NvMap::Handle::Id mem;
            u32 offset; //!< Offset in bytes from the handle of where the gather should start
            u32 words; //!< Size for the gather in 4 byte words
        };

        /**
         * @brief Describes a single memory relocation that can be applied to a pinned handle before command submission
         * @note These are used like: patchMem[patchOffset] = pinMem.iova + pinOffset
         */
        struct SubmitReloc {
            core::NvMap::Handle::Id patchMem;
            u32 patchOffset;
            core::NvMap::Handle::Id pinMem;
            u32 pinOffset;
        };

        /**
         * @brief Describes how the command buffers supplied with the submit will affect a given syncpoint
         */
        struct SubmitSyncpointIncr {
            u32 syncpointId;
            u32 numIncrs;
            std::array<u32, 3> _res_;
        };

        /**
         * @brief A buffer descriptor used for MapBuffer and UnmapBuffer
         */
        struct BufferHandle {
            core::NvMap::Handle::Id handle; //!< Handle to be (un)pinned
            u32 address; //!< The output IOVA that the handle was pinned too
        };

        Host1XChannel(const DeviceState &state,
                      Driver &driver,
                      Core &core,
                      const SessionContext &ctx,
                      core::ChannelType channelType);

        /**
         * @brief Sets the nvmap client to be used for channel submits
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_IOCTL_CHANNEL_SET_NVMAP_FD
         */
        PosixResult SetNvmapFd(In<FileDescriptor> id);

        /**
         * @brief Submits the specified command buffer data to the channel and returns fences that can be waited on
         * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CHANNEL_SUBMIT
         */
        PosixResult Submit(span<SubmitCmdBuf> cmdBufs,
                           span<SubmitReloc> relocs, span<u32> relocShifts,
                           span<SubmitSyncpointIncr> syncpointIncrs, span<u32> fenceThresholds);

        /**
         * @brief Returns the syncpoint ID that is located at the given index in this channel's syncpoint array
         * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CHANNEL_GET_SYNCPOINT
         */
        PosixResult GetSyncpoint(In<u32> channelSyncpointIdx, Out<u32> syncpointId);

        /**
         * @brief Stubbed in modern nvdrv to return 0
         * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CHANNEL_GET_WAITBASE
         */
        PosixResult GetWaitBase(In<core::ChannelType> pChannelType, Out<u32> waitBase);

        /**
         * @brief Sets the timeout for channel submits
         * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CHANNEL_SET_SUBMIT_TIMEOUT
         */
        PosixResult SetSubmitTimeout(In<u32> timeout);

        /**
         * @brief Pins a set of nvmap handles into the channel address space for use in submitted command buffers
         * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CHANNEL_MAP_CMD_BUFFER
         */
        PosixResult MapBuffer(u8 compressed, span<BufferHandle> handles);

        /**
         * @brief Unpins a set of nvmap handles into the channel address space
         * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CHANNEL_UNMAP_CMD_BUFFER
         */
        PosixResult UnmapBuffer(u8 compressed, span<BufferHandle> handles);

        PosixResult Ioctl(IoctlDescriptor cmd, span<u8> buffer) override;
    };
}
