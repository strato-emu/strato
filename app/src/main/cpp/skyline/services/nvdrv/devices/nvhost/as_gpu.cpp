// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <soc.h>
#include <services/nvdrv/devices/deserialisation/deserialisation.h>
#include "as_gpu.h"

namespace skyline::service::nvdrv::device::nvhost {
    AsGpu::AsGpu(const DeviceState &state, Core &core, const SessionContext &ctx) : NvDevice(state, core, ctx) {}

    PosixResult AsGpu::BindChannel(In<FileDescriptor> channelFd) {
        // TODO: support once multiple address spaces are supported
        return PosixResult::Success;
    }

    PosixResult AsGpu::AllocSpace(In<u32> pages, In<u32> pageSize, In<MappingFlags> flags, InOut<u64> offset) {
        // TODO: track this on the nvdrv side and have the gmmu only do virt -> phys
        // Also fix error codes
        u64 size{static_cast<u64>(pages) * static_cast<u64>(pageSize)};

        if (flags.fixed)
            offset = state.soc->gmmu.ReserveFixed(offset, size);
        else
            offset = state.soc->gmmu.ReserveSpace(size, offset); // offset contains the input alignment

        if (offset == 0) {
            state.logger->Warn("Failed to allocate GPU address space region!");
            return PosixResult::InvalidArgument;
        }

        return PosixResult::Success;
    }

    PosixResult AsGpu::FreeSpace(In<u64> offset, In<u32> pages, In<u32> pageSize) {
        // TODO: implement this when we add nvdrv side address space allocation
        return PosixResult::Success;
    }

    PosixResult AsGpu::UnmapBuffer(In<u64> offset) {
        try {
            auto region{regionMap.at(offset)};

            // Non-fixed regions are unmapped so that they can be used by future non-fixed mappings
            if (!region.fixed)
                if (!state.soc->gmmu.Unmap(offset, region.size))
                    state.logger->Warn("Failed to unmap region at 0x{:X}", offset);

            regionMap.erase(offset);
        } catch (const std::out_of_range &e) {
            state.logger->Warn("Couldn't find region to unmap at 0x{:X}", offset);
        }

        return PosixResult::Success;
    }

    PosixResult AsGpu::MapBufferEx(In<MappingFlags> flags, In<u32> kind, In<core::NvMap::Handle::Id> handle, InOut<u32> pageSize, In<u64> bufferOffset, In<u64> mappingSize, InOut<u64> offset) {
        state.logger->Debug("flags: ( fixed: {}, remap: {} ), kind: {}, handle: {}, pageSize: 0x{:X}, bufferOffset: 0x{:X}, mappingSize: 0x{:X}, offset: 0x{:X}", flags.fixed, flags.remap, kind, handle, pageSize, bufferOffset, mappingSize, offset);

        if (flags.remap) {
            auto region{regionMap.lower_bound(offset)};
            if (region == regionMap.end()) {
                state.logger->Warn("Cannot remap an unmapped GPU address space region: 0x{:X}", offset);
                return PosixResult::InvalidArgument;
            }

            if (region->second.size < mappingSize) {
                state.logger->Warn("Cannot remap an partially mapped GPU address space region: 0x{:X}", offset);
                return PosixResult::InvalidArgument;
            }

            u64 gpuAddress{offset + bufferOffset};
            u8 *cpuPtr{region->second.ptr + bufferOffset};

            if (!state.soc->gmmu.MapFixed(gpuAddress, cpuPtr, mappingSize)) {
                state.logger->Warn("Failed to remap GPU address space region: 0x{:X}", gpuAddress);
                return PosixResult::InvalidArgument;
            }

            return PosixResult::Success;
        }

        auto h{core.nvMap.GetHandle(handle)};
        if (!h)
            return PosixResult::InvalidArgument;

        if (auto err{h->Duplicate(ctx.internalSession)}; err != PosixResult::Success)
            return err;

        u8 *cpuPtr{reinterpret_cast<u8 *>(h->address + bufferOffset)};
        u64 size{mappingSize ? mappingSize : h->origSize};

        if (flags.fixed)
            offset = state.soc->gmmu.MapFixed(offset, cpuPtr, size);
        else
            offset = state.soc->gmmu.MapAllocate(cpuPtr, size);

        if (offset == 0) {
            state.logger->Warn("Failed to map GPU address space region!");
            return PosixResult::InvalidArgument;

        }

        state.logger->Debug("Mapped to 0x{:X}", offset);

        regionMap[offset] = {cpuPtr, size, flags.fixed};

        return PosixResult::Success;
    }

    PosixResult AsGpu::GetVaRegions(In<u64> bufAddr, InOut<u32> bufSize, Out<std::array<VaRegion, 2>> vaRegions) {
        // TODO: impl when we move allocator to nvdrv
        return PosixResult::Success;
    }

    PosixResult AsGpu::GetVaRegions3(span<u8> inlineBufer, In<u64> bufAddr, InOut<u32> bufSize, Out<std::array<VaRegion, 2>> vaRegions) {
        return GetVaRegions(bufAddr, bufSize, vaRegions);
    }

    PosixResult  AsGpu::AllocAsEx(In<u32> bigPageSize, In<FileDescriptor> asFd, In<u32> flags, In<u64> vaRangeStart, In<u64> vaRangeEnd, In<u64> vaRangeSplit) {
        // TODO: create the allocator here
        return PosixResult::Success;
    }

    PosixResult AsGpu::Remap(span<RemapEntry> entries) {
        constexpr u32 BigPageSize{0x10}; //!< The big page size of the GPU

        for (const auto &entry : entries) {
            auto h{core.nvMap.GetHandle(entry.handle)};
            if (!h)
                return PosixResult::InvalidArgument;

            u64 virtAddr{static_cast<u64>(entry.asOffsetBigPages) << BigPageSize};
            u8 *cpuPtr{reinterpret_cast<u8 *>(h->address + (static_cast<u64>(entry.handleOffsetBigPages) << BigPageSize))};
            u64 size{static_cast<u64>(entry.bigPages) << BigPageSize};

            state.soc->gmmu.MapFixed(virtAddr, cpuPtr, size);
        }

        return PosixResult::Success;
    }

#include <services/nvdrv/devices/deserialisation/macro_def.h>
    static constexpr u32 AsGpuMagic{0x41};

    VARIABLE_IOCTL_HANDLER_FUNC(AsGpu, ({
        IOCTL_CASE_ARGS(IN,    SIZE(0x4),  MAGIC(AsGpuMagic), FUNC(0x1),
                        BindChannel,  ARGS(In<FileDescriptor>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x18), MAGIC(AsGpuMagic), FUNC(0x2),
                        AllocSpace,   ARGS(In<u32>, In<u32>, In<MappingFlags>, Pad<u32>, InOut<u64>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x10), MAGIC(AsGpuMagic), FUNC(0x3),
                        FreeSpace,    ARGS(In<u64>, In<u32>, In<u32>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x8),  MAGIC(AsGpuMagic), FUNC(0x5),
                        UnmapBuffer,  ARGS(In<u64>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x28), MAGIC(AsGpuMagic), FUNC(0x6),
                        MapBufferEx,  ARGS(In<MappingFlags>, In<u32>, In<core::NvMap::Handle::Id>, InOut<u32>, In<u64>, In<u64>, InOut<u64>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x40), MAGIC(AsGpuMagic), FUNC(0x8),
                        GetVaRegions, ARGS(In<u64>, InOut<u32>, Pad<u32>, Out<std::array<VaRegion, 2>>))
        IOCTL_CASE_ARGS(IN,    SIZE(0x28), MAGIC(AsGpuMagic), FUNC(0x9),
                        AllocAsEx,    ARGS(In<u32>, In<FileDescriptor>, In<u32>, Pad<u32>, In<u64>, In<u64>, In<u64>))
    }), ({
        VARIABLE_IOCTL_CASE_ARGS(INOUT, MAGIC(AsGpuMagic), FUNC(0x14),
                                 Remap, ARGS(AutoSizeSpan<RemapEntry>))
    }))

    INLINE_IOCTL_HANDLER_FUNC(Ioctl3, AsGpu, ({
        INLINE_IOCTL_CASE_ARGS(INOUT, SIZE(0x40), MAGIC(AsGpuMagic), FUNC(0x8),
                               GetVaRegions3, ARGS(In<u64>, InOut<u32>, Pad<u32>, Out<std::array<VaRegion, 2>>))
    }))
#include <services/nvdrv/devices/deserialisation/macro_undef.h>
}
