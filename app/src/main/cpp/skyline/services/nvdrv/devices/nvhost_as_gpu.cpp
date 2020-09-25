// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <os.h>
#include <kernel/types/KProcess.h>
#include <services/nvdrv/driver.h>
#include "nvmap.h"
#include "nvhost_as_gpu.h"

namespace skyline::service::nvdrv::device {
    NvHostAsGpu::NvHostAsGpu(const DeviceState &state) : NvDevice(state) {}

    NvStatus NvHostAsGpu::BindChannel(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        return NvStatus::Success;
    }

    NvStatus NvHostAsGpu::AllocSpace(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct Data {
            u32 pages;      // In
            u32 pageSize;   // In
            u32 flags;      // In
            u32 _pad_;
            union {
                u64 offset; // InOut
                u64 align;  // In
            };
        } region = buffer.as<Data>();

        u64 size = static_cast<u64>(region.pages) * static_cast<u64>(region.pageSize);

        if (region.flags & 1)
            region.offset = state.gpu->memoryManager.ReserveFixed(region.offset, size);
        else
            region.offset = state.gpu->memoryManager.ReserveSpace(size);

        if (region.offset == 0) {
            state.logger->Warn("Failed to allocate GPU address space region!");
            return NvStatus::BadParameter;
        }

        return NvStatus::Success;
    }

    NvStatus NvHostAsGpu::UnmapBuffer(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        u64 offset{buffer.as<u64>()};

        if (!state.gpu->memoryManager.Unmap(offset))
            state.logger->Warn("Failed to unmap chunk at 0x{:X}", offset);

        return NvStatus::Success;
    }

    NvStatus NvHostAsGpu::Modify(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        struct Data {
            u32 flags;        // In
            u32 kind;         // In
            u32 nvmapHandle;  // In
            u32 pageSize;     // InOut
            u64 bufferOffset; // In
            u64 mappingSize;  // In
            u64 offset;       // InOut
        }  &data = buffer.as<Data>();

        try {
            auto driver = nvdrv::driver.lock();
            auto nvmap = driver->nvMap.lock();
            auto mapping = nvmap->handleTable.at(data.nvmapHandle);

            u64 mapPhysicalAddress = data.bufferOffset + mapping->address;
            u64 mapSize = data.mappingSize ? data.mappingSize : mapping->size;

            if (data.flags & 1)
                data.offset = state.gpu->memoryManager.MapFixed(data.offset, mapPhysicalAddress, mapSize);
            else
                data.offset = state.gpu->memoryManager.MapAllocate(mapPhysicalAddress, mapSize);

            if (data.offset == 0) {
                state.logger->Warn("Failed to map GPU address space region!");
                return NvStatus::BadParameter;
            }

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
        for (auto entry : entries) {
            try {
                auto driver = nvdrv::driver.lock();
                auto nvmap = driver->nvMap.lock();
                auto mapping = nvmap->handleTable.at(entry.nvmapHandle);

                u64 mapAddress = static_cast<u64>(entry.gpuOffset) << MinAlignmentShift;
                u64 mapPhysicalAddress = mapping->address + (static_cast<u64>(entry.mapOffset) << MinAlignmentShift);
                u64 mapSize = static_cast<u64>(entry.pages) << MinAlignmentShift;

                state.gpu->memoryManager.MapFixed(mapAddress, mapPhysicalAddress, mapSize);
            } catch (const std::out_of_range &) {
                state.logger->Warn("Invalid NvMap handle: 0x{:X}", entry.nvmapHandle);
                return NvStatus::BadParameter;
            }
        }

        return NvStatus::Success;
    }
}
