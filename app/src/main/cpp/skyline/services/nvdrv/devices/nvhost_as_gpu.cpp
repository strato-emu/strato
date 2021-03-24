// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <soc.h>
#include <services/nvdrv/driver.h>
#include "nvmap.h"
#include "nvhost_as_gpu.h"

namespace skyline::service::nvdrv::device {
    struct MappingFlags {
        bool fixed : 1;
        u8 _pad0_ : 7;
        bool remap : 1;
        u32 _pad1_ : 23;
    };
    static_assert(sizeof(MappingFlags) == sizeof(u32));

    NvHostAsGpu::NvHostAsGpu(const DeviceState &state) : NvDevice(state) {}

    NvStatus NvHostAsGpu::BindChannel(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        return NvStatus::Success;
    }

    NvStatus NvHostAsGpu::AllocSpace(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct Data {
            u32 pages;          // In
            u32 pageSize;       // In
            MappingFlags flags; // In
            u32 _pad_;
            union {
                u64 offset;     // InOut
                u64 align;      // In
            };
        } &region = buffer.as<Data>();

        u64 size{static_cast<u64>(region.pages) * static_cast<u64>(region.pageSize)};

        if (region.flags.fixed)
            region.offset = state.soc->gmmu.ReserveFixed(region.offset, size);
        else
            region.offset = state.soc->gmmu.ReserveSpace(size, region.align);

        if (region.offset == 0) {
            state.logger->Warn("Failed to allocate GPU address space region!");
            return NvStatus::BadParameter;
        }

        return NvStatus::Success;
    }

    NvStatus NvHostAsGpu::UnmapBuffer(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        u64 offset{buffer.as<u64>()};

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

        return NvStatus::Success;
    }

    NvStatus NvHostAsGpu::Modify(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct Data {
            MappingFlags flags; // In
            u32 kind;           // In
            u32 nvmapHandle;    // In
            u32 pageSize;       // InOut
            u64 bufferOffset;   // In
            u64 mappingSize;    // In
            u64 offset;         // InOut
        } &data = buffer.as<Data>();

        try {
            if (data.flags.remap) {
                auto region{regionMap.lower_bound(data.offset)};
                if (region == regionMap.end()) {
                    state.logger->Warn("Cannot remap an unmapped GPU address space region: 0x{:X}", data.offset);
                    return NvStatus::BadParameter;
                }

                if (region->second.size < data.mappingSize) {
                    state.logger->Warn("Cannot remap an partially mapped GPU address space region: 0x{:X}", data.offset);
                    return NvStatus::BadParameter;
                }

                u64 gpuAddress{data.offset + data.bufferOffset};
                u8 *cpuPtr{region->second.ptr + data.bufferOffset};

                if (!state.soc->gmmu.MapFixed(gpuAddress, cpuPtr, data.mappingSize)) {
                    state.logger->Warn("Failed to remap GPU address space region: 0x{:X}", gpuAddress);
                    return NvStatus::BadParameter;
                }

                return NvStatus::Success;
            }

            auto driver{nvdrv::driver.lock()};
            auto nvmap{driver->nvMap.lock()};
            auto mapping{nvmap->GetObject(data.nvmapHandle)};

            u8 *cpuPtr{data.bufferOffset + mapping->ptr};
            u64 size{data.mappingSize ? data.mappingSize : mapping->size};

            if (data.flags.fixed)
                data.offset = state.soc->gmmu.MapFixed(data.offset, cpuPtr, size);
            else
                data.offset = state.soc->gmmu.MapAllocate(cpuPtr, size);

            if (data.offset == 0) {
                state.logger->Warn("Failed to map GPU address space region!");
                return NvStatus::BadParameter;
            }

            regionMap[data.offset] = {cpuPtr, size, data.flags.fixed};

            return NvStatus::Success;
        } catch (const std::out_of_range &) {
            state.logger->Warn("Invalid NvMap handle: 0x{:X}", data.nvmapHandle);
            return NvStatus::BadParameter;
        }
    }

    NvStatus NvHostAsGpu::GetVaRegions(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        /*
        struct Data {
            u64 _pad0_;
            u32 bufferSize; // InOut
            u32 _pad1_;

            struct {
                u64 offset;
                u32 page_size;
                u32 pad;
                u64 pages;
            } regions[2];   // Out
        } &regionInfo = buffer.as<Data>();
        */
        return NvStatus::Success;
    }

    NvStatus NvHostAsGpu::AllocAsEx(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        /*
        struct Data {
            u32 bigPageSize;  // In
            i32 asFd;         // In
            u32 flags;        // In
            u32 reserved;     // In
            u64 vaRangeStart; // In
            u64 vaRangeEnd;   // In
            u64 vaRangeSplit; // In
        } addressSpace = buffer.as<Data>();
        */
        return NvStatus::Success;
    }

    NvStatus NvHostAsGpu::Remap(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct Entry {
            u16 flags;       // In
            u16 kind;        // In
            u32 nvmapHandle; // In
            u32 mapOffset;   // In
            u32 gpuOffset;   // In
            u32 pages;       // In
        };

        constexpr u32 MinAlignmentShift{0x10}; // This shift is applied to all addresses passed to Remap

        auto entries{buffer.cast<Entry>()};
        for (const auto &entry : entries) {
            try {
                auto driver{nvdrv::driver.lock()};
                auto nvmap{driver->nvMap.lock()};
                auto mapping{nvmap->GetObject(entry.nvmapHandle)};

                u64 virtAddr{static_cast<u64>(entry.gpuOffset) << MinAlignmentShift};
                u8 *cpuPtr{mapping->ptr + (static_cast<u64>(entry.mapOffset) << MinAlignmentShift)};
                u64 size{static_cast<u64>(entry.pages) << MinAlignmentShift};

                state.soc->gmmu.MapFixed(virtAddr, cpuPtr, size);
            } catch (const std::out_of_range &) {
                state.logger->Warn("Invalid NvMap handle: 0x{:X}", entry.nvmapHandle);
                return NvStatus::BadParameter;
            }
        }

        return NvStatus::Success;
    }
}
