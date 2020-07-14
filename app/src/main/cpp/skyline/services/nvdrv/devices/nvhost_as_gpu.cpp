// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <os.h>
#include <kernel/types/KProcess.h>
#include <services/nvdrv/INvDrvServices.h>
#include "nvmap.h"
#include "nvhost_as_gpu.h"

namespace skyline::service::nvdrv::device {
    NvHostAsGpu::NvHostAsGpu(const DeviceState &state) : NvDevice(state, NvDeviceType::nvhost_as_gpu, {
        {0x4109, NFUNC(NvHostAsGpu::InitializeEx)},
        {0x4108, NFUNC(NvHostAsGpu::GetVaRegions)},
        {0x4102, NFUNC(NvHostAsGpu::AllocSpace)},
        {0x4106, NFUNC(NvHostAsGpu::Modify)},
        {0x4101, NFUNC(NvHostAsGpu::BindChannel)},
        {0x4114, NFUNC(NvHostAsGpu::BindChannel)}
    }) {}

    void NvHostAsGpu::InitializeEx(IoctlData &buffer) {
        struct Data {
            u32 bigPageSize;
            i32 asFd;
            u32 flags;
            u32 reserved;
            u64 vaRangeStart;
            u64 vaRangeEnd;
            u64 vaRangeSplit;
        } addressSpace = state.process->GetObject<Data>(buffer.input.at(0).address);
    }

    void NvHostAsGpu::GetVaRegions(IoctlData &buffer) {
        struct Data {
            u64 _pad0_;
            u32 bufferSize;
            u32 _pad1_;

            struct {
                u64 offset;
                u32 page_size;
                u32 pad;
                u64 pages;
            } regions[2];
        } regionInfo = state.process->GetReference<Data>(buffer.input.at(0).address);
        state.process->WriteMemory(regionInfo, buffer.output.at(0).address);
    }

    void NvHostAsGpu::BindChannel(IoctlData &buffer) {
        struct Data {
            u32 fd;
        } channelInfo = state.process->GetReference<Data>(buffer.input.at(0).address);
    }

    void NvHostAsGpu::AllocSpace(IoctlData &buffer) {
        struct Data {
            u32 pages;
            u32 pageSize;
            u32 flags;
            u32 _pad_;
            u64 offset;
        } region = state.process->GetObject<Data>(buffer.input.at(0).address);

        u64 size = static_cast<u64>(region.pages) * static_cast<u64>(region.pageSize);

        if (region.flags & 1)
            region.offset = state.gpu->memoryManager.AllocateFixed(region.offset, size);
        else
            region.offset = state.gpu->memoryManager.AllocateSpace(size);

        if (region.offset == 0) {
            state.logger->Warn("Failed to allocate GPU address space region!");
            buffer.status = NvStatus::BadParameter;
        }

        state.process->WriteMemory(region, buffer.output.at(0).address);
    }

    void NvHostAsGpu::Modify(IoctlData &buffer) {
        struct Data {
            u32 flags;
            u32 kind;
            u32 nvmapHandle;
            u32 pageSize;
            u64 bufferOffset;
            u64 mappingSize;
            u64 offset;
        } region = state.process->GetObject<Data>(buffer.input.at(0).address);

        if (!region.nvmapHandle)
            return;

        auto nvmap = state.os->serviceManager.GetService<nvdrv::INvDrvServices>(Service::nvdrv_INvDrvServices)->GetDevice<nvdrv::device::NvMap>(nvdrv::device::NvDeviceType::nvmap)->handleTable.at(region.nvmapHandle);

        u64 mapPhysicalAddress = region.bufferOffset + nvmap->address;
        u64 mapSize = region.mappingSize ? region.mappingSize : nvmap->size;

        if (region.flags & 1)
            region.offset = state.gpu->memoryManager.MapFixed(region.offset, mapPhysicalAddress, mapSize);
        else
            region.offset = state.gpu->memoryManager.MapAllocated(mapPhysicalAddress, mapSize);

        if (region.offset == 0) {
            state.logger->Warn("Failed to map GPU address space region!");
            buffer.status = NvStatus::BadParameter;
        }

        state.process->WriteMemory(region, buffer.output.at(0).address);
    }
}
