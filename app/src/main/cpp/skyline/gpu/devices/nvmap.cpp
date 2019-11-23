#include "nvmap.h"
#include <kernel/types/KProcess.h>

namespace skyline::gpu::device {
    NvMap::NvMapObject::NvMapObject(u32 id, u32 size) : id(id), size(size) {}

    NvMap::NvMap(const DeviceState &state) : NvDevice(state, NvDeviceType::nvmap, {
        {0xC0080101, NFUNC(NvMap::Create)},
        {0xC0080103, NFUNC(NvMap::FromId)},
        {0xC0200104, NFUNC(NvMap::Alloc)},
        {0xC0180105, NFUNC(NvMap::Free)},
        {0xC00C0109, NFUNC(NvMap::Param)},
        {0xC008010E, NFUNC(NvMap::GetId)}
    }) {}

    void NvMap::Create(IoctlData &buffer) {
        struct Data {
            u32 size;   // In
            u32 handle; // Out
        } data = state.thisProcess->ReadMemory<Data>(buffer.input[0].address);
        handleTable[handleIndex] = std::make_shared<NvMapObject>(idIndex++, data.size);
        data.handle = handleIndex++;
        state.thisProcess->WriteMemory(data, buffer.output[0].address);
        state.logger->Debug("Create: Input: Size: 0x{:X}, Output: Handle: 0x{:X}, Status: {}", data.size, data.handle, buffer.status);
    }

    void NvMap::FromId(skyline::gpu::device::IoctlData &buffer) {
        struct Data {
            u32 id;     // In
            u32 handle; // Out
        } data = state.thisProcess->ReadMemory<Data>(buffer.input[0].address);
        bool found{};
        for (const auto &object : handleTable) {
            if (object.second->id == data.id) {
                data.handle = object.first;
                found = true;
                break;
            }
        }
        if (found)
            state.thisProcess->WriteMemory(data, buffer.output[0].address);
        else
            buffer.status = NvStatus::BadValue;
        state.logger->Debug("FromId: Input: Handle: 0x{:X}, Output: ID: 0x{:X}, Status: {}", data.handle, data.id, buffer.status);
    }

    void NvMap::Alloc(IoctlData &buffer) {
        struct Data {
            u32 handle;   // In
            u32 heapMask; // In
            u32 flags;    // In
            u32 align;    // In
            u8 kind;     // In
            u8 _pad0_[7];
            u64 address;  // InOut
        } data = state.thisProcess->ReadMemory<Data>(buffer.input[0].address);
        auto &object = handleTable.at(data.handle);
        object->heapMask = data.heapMask;
        object->flags = data.flags;
        object->align = data.align;
        object->kind = data.kind;
        object->address = data.address;
        object->status = NvMapObject::Status::Allocated;
        state.logger->Debug("Alloc: Input: Handle: 0x{:X}, HeapMask: 0x{:X}, Flags: {}, Align: 0x{:X}, Kind: {}, Address: 0x{:X}, Output: Status: {}", data.handle, data.heapMask, data.flags, data.align, data.kind, data.address, buffer.status);
    }

    void NvMap::Free(skyline::gpu::device::IoctlData &buffer) {
        struct Data {
            u32 handle;   // In
            u32 _pad0_;
            u32 address;  // Out
            u32 size;     // Out
            u64 flags;    // Out
        } data = state.thisProcess->ReadMemory<Data>(buffer.input[0].address);
        const auto &object = handleTable.at(data.handle);
        if (object.use_count() > 1) {
            data.address = static_cast<u32>(object->address);
            data.flags = 0x0;
        } else {
            data.address = 0x0;
            data.flags = 0x1; // Not free yet
        }
        data.size = object->size;
        handleTable.erase(data.handle);
        state.thisProcess->WriteMemory(data, buffer.output[0].address);
    }

    void NvMap::Param(IoctlData &buffer) {
        enum class Parameter : u32 { Size = 1, Alignment = 2, Base = 3, HeapMask = 4, Kind = 5, Compr = 6 }; // https://android.googlesource.com/kernel/tegra/+/refs/heads/android-tegra-flounder-3.10-marshmallow/include/linux/nvmap.h#102
        struct Data {
            u32 handle;          // In
            Parameter parameter; // In
            u32 result;          // Out
        } data = state.thisProcess->ReadMemory<Data>(buffer.input[0].address);
        auto &object = handleTable.at(data.handle);
        switch (data.parameter) {
            case Parameter::Size:
                data.result = object->size;
                break;
            case Parameter::Alignment:
            case Parameter::HeapMask:
            case Parameter::Kind: {
                if (object->status != NvMapObject::Status::Allocated)
                    data.result = NvStatus::BadParameter;
                switch (data.parameter) {
                    case Parameter::Alignment:
                        data.result = object->align;
                        break;
                    case Parameter::HeapMask:
                        data.result = object->heapMask;
                        break;
                    case Parameter::Kind:
                        data.result = object->kind;
                        break;
                    default:
                        break;
                }
                break;
            }
            case Parameter::Base:
                buffer.status = NvStatus::NotImplemented;
                break;
            case Parameter::Compr:
                buffer.status = NvStatus::NotImplemented;
                break;
        }
        state.thisProcess->WriteMemory(data, buffer.output[0].address);
        state.logger->Debug("Param: Input: Handle: 0x{:X}, Parameter: {}, Output: Result: 0x{:X}, Status: {}", data.handle, data.parameter, data.result, buffer.status);
    }

    void NvMap::GetId(skyline::gpu::device::IoctlData &buffer) {
        struct Data {
            u32 id;     // Out
            u32 handle; // In
        } data = state.thisProcess->ReadMemory<Data>(buffer.input[0].address);
        data.id = handleTable.at(data.handle)->id;
        state.thisProcess->WriteMemory(data, buffer.output[0].address);
        state.logger->Debug("GetId: Input: Handle: 0x{:X}, Output: ID: 0x{:X}, Status: {}", data.handle, data.id, buffer.status);
    }
}
