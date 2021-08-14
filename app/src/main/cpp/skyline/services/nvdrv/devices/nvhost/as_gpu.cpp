// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/address_space.inc>
#include <soc.h>
#include <services/nvdrv/devices/deserialisation/deserialisation.h>
#include "as_gpu.h"

namespace skyline {
    template class FlatAddressSpaceMap<u32, 0, bool, false, false, 32>;
    template class FlatAllocator<u32, 0, 32>;
}

namespace skyline::service::nvdrv::device::nvhost {
    AsGpu::AsGpu(const DeviceState &state, Core &core, const SessionContext &ctx) : NvDevice(state, core, ctx) {}

    PosixResult AsGpu::BindChannel(In<FileDescriptor> channelFd) {
        // TODO: support once multiple address spaces are supported
        return PosixResult::Success;
    }

    PosixResult AsGpu::AllocSpace(In<u32> pages, In<u32> pageSize, In<MappingFlags> flags, InOut<u64> offset) {
        state.logger->Debug("pages: 0x{:X}, pageSize: 0x{:X}, flags: ( fixed: {}, sparse: {} ), offset: 0x{:X}", pages, pageSize, flags.fixed, flags.sparse, offset);

        if (pageSize != VM::PageSize && pageSize != vm.bigPageSize)
            return PosixResult::InvalidArgument;

        if (pageSize != vm.bigPageSize && flags.sparse)
            return PosixResult::FunctionNotImplemented;

        u32 pageSizeBits{pageSize == VM::PageSize ? VM::PageSizeBits : vm.bigPageSizeBits};

        auto &allocator{[&] () -> auto & {
            if (pageSize == VM::PageSize)
                return vm.smallPageAllocator;
            else
                return vm.bigPageAllocator;
        }()};

        if (flags.fixed)
            allocator->AllocateFixed(offset >> pageSizeBits, pages);
        else
            offset = static_cast<u64>(allocator->Allocate(pages)) << pageSizeBits;

        u64 size{static_cast<u64>(pages) * static_cast<u64>(pageSize)};

        if (flags.sparse)
            state.soc->gm20b.gmmu.Map(offset, soc::gm20b::GM20B::GMMU::SparsePlaceholderAddress(), size, true);

        allocationMap[offset] = {
            .size = size,
            .pageSize = pageSize,
            .sparse = flags.sparse
        };

        return PosixResult::Success;
    }

    PosixResult AsGpu::FreeSpace(In<u64> offset, In<u32> pages, In<u32> pageSize) {
        // TODO: implement after UNMAP
        return PosixResult::Success;
    }

    PosixResult AsGpu::UnmapBuffer(In<u64> offset) {
        state.logger->Debug("offset: 0x{:X}", offset);

        try {
            auto mapping{mappingMap.at(offset)};

            if (!mapping->fixed) {
                auto &allocator{mapping->bigPage ? vm.bigPageAllocator : vm.smallPageAllocator};
                u32 pageSizeBits{mapping->bigPage ? vm.bigPageSizeBits : VM::PageSizeBits};

                allocator->Free(mapping->offset >> pageSizeBits, mapping->size >> pageSizeBits);
            }

            if (mapping->sparseAlloc)
                state.soc->gm20b.gmmu.Map(offset, soc::gm20b::GM20B::GMMU::SparsePlaceholderAddress(), mapping->size, true);
            else
                state.soc->gm20b.gmmu.Unmap(offset, mapping->size);

            mappingMap.erase(offset);
        } catch (const std::out_of_range &e) {
            state.logger->Warn("Couldn't find region to unmap at 0x{:X}", offset);
        }

        return PosixResult::Success;
    }

    PosixResult AsGpu::MapBufferEx(In<MappingFlags> flags, In<u32> kind, In<core::NvMap::Handle::Id> handle, In<u64> bufferOffset, In<u64> mappingSize, InOut<u64> offset) {
        if (!vm.initialised)
            return PosixResult::InvalidArgument;

        state.logger->Debug("flags: ( fixed: {}, remap: {} ), kind: {}, handle: {}, bufferOffset: 0x{:X}, mappingSize: 0x{:X}, offset: 0x{:X}", flags.fixed, flags.remap, kind, handle, bufferOffset, mappingSize, offset);

        if (flags.remap) {
            try {
                auto mapping{mappingMap.at(offset)};

                if (mapping->size < mappingSize) {
                    state.logger->Warn("Cannot remap a partially mapped GPU address space region: 0x{:X}", offset);
                    return PosixResult::InvalidArgument;
                }

                u64 gpuAddress{offset + bufferOffset};
                u8 *cpuPtr{mapping->ptr + bufferOffset};

                state.soc->gm20b.gmmu.Map(gpuAddress, cpuPtr, mappingSize);

                return PosixResult::Success;
            } catch (const std::out_of_range &e) {
                state.logger->Warn("Cannot remap an unmapped GPU address space region: 0x{:X}", offset);
                return PosixResult::InvalidArgument;
            }
        }

        auto h{core.nvMap.GetHandle(handle)};
        if (!h)
            return PosixResult::InvalidArgument;

        u8 *cpuPtr{reinterpret_cast<u8 *>(h->address + bufferOffset)};
        u64 size{mappingSize ? mappingSize : h->origSize};

        if (flags.fixed) {
            auto alloc{allocationMap.upper_bound(offset)};

            if (alloc-- == allocationMap.begin() || (offset - alloc->first) + size > alloc->second.size)
                throw exception("Cannot perform a fixed mapping into an unallocated region!");

            state.soc->gm20b.gmmu.Map(offset, cpuPtr, size);

            auto mapping{std::make_shared<Mapping>(cpuPtr, offset, size, true, false, alloc->second.sparse)};
            alloc->second.mappings.push_back(mapping);
            mappingMap[offset] = mapping;
        } else {
            bool bigPage{[&] () {
                if (util::IsAligned(h->align, vm.bigPageSize))
                    return true;
                else if (util::IsAligned(h->align, VM::PageSize))
                    return false;
                else
                    throw exception("Invalid handle alignment: 0x{:X}", h->align);
            }()};

            auto &allocator{bigPage ? vm.bigPageAllocator : vm.smallPageAllocator};
            u32 pageSize{bigPage ? vm.bigPageSize : VM::PageSize};
            u32 pageSizeBits{bigPage ? vm.bigPageSizeBits : VM::PageSizeBits};

            offset = static_cast<u64>(allocator->Allocate(util::AlignUp(size, pageSize) >> pageSizeBits)) << pageSizeBits;
            state.soc->gm20b.gmmu.Map(offset, cpuPtr, size);

            auto mapping{std::make_shared<Mapping>(cpuPtr, offset, size, false, bigPage, false)};
            mappingMap[offset] = mapping;
        }

        state.logger->Debug("Mapped to 0x{:X}", offset);

        return PosixResult::Success;
    }

    PosixResult AsGpu::GetVaRegions(In<u64> bufAddr, InOut<u32> bufSize, Out<std::array<VaRegion, 2>> vaRegions) {
        if (!vm.initialised)
            return PosixResult::InvalidArgument;

        vaRegions = std::array<VaRegion, 2> {
            VaRegion{
                .pageSize = VM::PageSize,
                .pages = vm.smallPageAllocator->vaLimit - vm.smallPageAllocator->vaStart,
                .offset = vm.smallPageAllocator->vaStart << VM::PageSizeBits,
            },
            VaRegion{
                .pageSize = vm.bigPageSize,
                .pages = vm.bigPageAllocator->vaLimit - vm.bigPageAllocator->vaStart,
                .offset = vm.bigPageAllocator->vaStart << vm.bigPageSizeBits,
            }
        };

        return PosixResult::Success;
    }

    PosixResult AsGpu::GetVaRegions3(span<u8> inlineBufer, In<u64> bufAddr, InOut<u32> bufSize, Out<std::array<VaRegion, 2>> vaRegions) {
        return GetVaRegions(bufAddr, bufSize, vaRegions);
    }

    PosixResult AsGpu::AllocAsEx(In<u32> flags, In<FileDescriptor> asFd, In<u32> bigPageSize, In<u64> vaRangeStart, In<u64> vaRangeEnd, In<u64> vaRangeSplit) {
        if (vm.initialised)
            throw exception("Cannot initialise an address space twice!");

        state.logger->Debug("bigPageSize: 0x{:X}, asFd: {}, flags: 0x{:X}, vaRangeStart: 0x{:X}, vaRangeEnd: 0x{:X}, vaRangeSplit: 0x{:X}",
                            bigPageSize, asFd, flags, vaRangeStart, vaRangeEnd, vaRangeSplit);

        if (bigPageSize) {
            if (!std::ispow2(bigPageSize)) {
                state.logger->Error("Non power-of-2 big page size: 0x{:X}!", bigPageSize);
                return PosixResult::InvalidArgument;
            }

            if (!(bigPageSize & VM::SupportedBigPageSizes)) {
                state.logger->Error("Unsupported big page size: 0x{:X}!", bigPageSize);
                return PosixResult::InvalidArgument;
            }

            vm.bigPageSize = bigPageSize;
            vm.bigPageSizeBits = std::countr_zero(bigPageSize);

            vm.vaRangeStart = bigPageSize << VM::VaStartShift;
        }

        if (vaRangeStart) {
            vm.vaRangeStart = vaRangeStart;
            vm.vaRangeSplit = vaRangeSplit;
            vm.vaRangeEnd = vaRangeEnd;
        }

        u64 startPages{vm.vaRangeStart >> VM::PageSizeBits};
        u64 endPages{vm.vaRangeSplit >> VM::PageSizeBits};
        vm.smallPageAllocator = std::make_unique<VM::Allocator>(startPages, endPages);

        u64 startBigPages{vm.vaRangeSplit >> vm.bigPageSizeBits};
        u64 endBigPages{(vm.vaRangeEnd - vm.vaRangeSplit) >> vm.bigPageSizeBits};
        vm.bigPageAllocator = std::make_unique<VM::Allocator>(startBigPages, endBigPages);

        vm.initialised = true;

        return PosixResult::Success;
    }

    PosixResult AsGpu::Remap(span<RemapEntry> entries) {
        for (const auto &entry : entries) {
            u64 virtAddr{static_cast<u64>(entry.asOffsetBigPages) << vm.bigPageSizeBits};
            u64 size{static_cast<u64>(entry.bigPages) << vm.bigPageSizeBits};

            auto alloc{allocationMap.upper_bound(virtAddr)};

            if (alloc-- == allocationMap.begin() || (virtAddr - alloc->first) + size > alloc->second.size) {
                state.logger->Warn("Cannot remap into an unallocated region!");
                return PosixResult::InvalidArgument;
            }

            if (!alloc->second.sparse) {
                state.logger->Warn("Cannot remap a non-sparse mapping!");
                return PosixResult::InvalidArgument;
            }

            if (!entry.handle) {
                state.soc->gm20b.gmmu.Map(virtAddr, soc::gm20b::GM20B::GMMU::SparsePlaceholderAddress(), size, true);
            } else {
                auto h{core.nvMap.GetHandle(entry.handle)};
                if (!h)
                    return PosixResult::InvalidArgument;

                u8 *cpuPtr{reinterpret_cast<u8 *>(h->address + (static_cast<u64>(entry.handleOffsetBigPages) << vm.bigPageSizeBits))};

                state.soc->gm20b.gmmu.Map(virtAddr, cpuPtr, size);
            }
        }

        return PosixResult::Success;
    }

#include <services/nvdrv/devices/deserialisation/macro_def.inc>
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
                        MapBufferEx,  ARGS(In<MappingFlags>, In<u32>, In<core::NvMap::Handle::Id>, Pad<u32>, In<u64>, In<u64>, InOut<u64>))
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
#include <services/nvdrv/devices/deserialisation/macro_undef.inc>
}
