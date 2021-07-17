// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/nvdrv/devices/nvdevice.h>

namespace skyline::service::nvdrv::device::nvhost {
    /**
     * @brief nvhost::AsGpu (/dev/nvhost-as-gpu) is used to access a GPU virtual address space
     * @url https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-as-gpu
     */
    class AsGpu : public NvDevice {
      private:
        struct AddressSpaceRegion {
            u8 *ptr;
            u64 size;
            bool fixed;
        };

        std::map<u64, AddressSpaceRegion> regionMap; //!< This maps the base addresses of mapped buffers to their total sizes and mapping type, this is needed as what was originally a single buffer may have been split into multiple GPU side buffers with the remap flag.

      public:
        struct MappingFlags {
            bool fixed : 1;
            u8 _pad0_ : 7;
            bool remap : 1;
            u32 _pad1_ : 23;
        };
        static_assert(sizeof(MappingFlags) == sizeof(u32));

        struct VaRegion {
            u64 offset;
            u32 pageSize;
            u32 _pad0_;
            u64 pages;
        };
        static_assert(sizeof(VaRegion) == 0x18);

        struct RemapEntry {
            u16 flags;
            u16 kind;
            core::NvMap::Handle::Id handle;
            u32 handleOffsetBigPages;
            u32 asOffsetBigPages;
            u32 bigPages;
        };
        static_assert(sizeof(RemapEntry) == 0x14);

        AsGpu(const DeviceState &state, Core &core, const SessionContext &ctx);

        /**
         * @brief Binds this address space to a channel
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_AS_IOCTL_BIND_CHANNEL
         */
        PosixResult BindChannel(In<FileDescriptor> channelFd);

        /**
         * @brief Reserves a region in this address space
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_AS_IOCTL_FREE_SPACE
         */
        PosixResult AllocSpace(In<u32> pages, In<u32> pageSize, In<MappingFlags> flags, InOut<u64> offset);

        /**
         * @brief Frees an allocated region in this address space
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_AS_IOCTL_FREE_SPACE
         */
        PosixResult FreeSpace(In<u64> offset, In<u32> pages, In<u32> pageSize);

        /**
         * @brief Unmaps a region in this address space
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_AS_IOCTL_UNMAP_BUFFER
         */
        PosixResult UnmapBuffer(In<u64> offset);

        /**
         * @brief Maps a region into this address space with extra parameters
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_AS_IOCTL_MAP_BUFFER_EX
         */
        PosixResult MapBufferEx(In<MappingFlags> flags, In<u32> kind, In<core::NvMap::Handle::Id> handle, InOut<u32> pageSize, In<u64> bufferOffset, In<u64> mappingSize, InOut<u64> offset);

        /**
         * @brief Returns info about the address space and its page sizes
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_AS_IOCTL_GET_VA_REGIONS
         */
        PosixResult GetVaRegions(In<u64> bufAddr, InOut<u32> bufSize, Out<std::array<VaRegion, 2>> vaRegions);

        /**
         * @brief Allocates this address space with the given parameters
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_AS_IOCTL_ALLOC_AS_EX
         */
        PosixResult AllocAsEx(In<u32> bigPageSize, In<FileDescriptor> asFd, In<u32> flags, In<u64> vaRangeStart, In<u64> vaRangeEnd, In<u64> vaRangeSplit);

        /**
         * @brief Remaps a region of the GPU address space
         * @url https://switchbrew.org/wiki/NV_services#NVGPU_AS_IOCTL_REMAP
         */
        PosixResult Remap(span<RemapEntry> entries);

        PosixResult Ioctl(IoctlDescriptor cmd, span<u8> buffer) override;
    };
}
