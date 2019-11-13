#include "parcel.h"
#include <os.h>
#include <kernel/types/KProcess.h>

namespace skyline::gpu {
    Parcel::Parcel(kernel::ipc::BufferDescriptorABW *buffer, const DeviceState &state) : Parcel(buffer->Address(), buffer->Size(), state) {}

    Parcel::Parcel(kernel::ipc::BufferDescriptorX *buffer, const DeviceState &state) : Parcel(buffer->Address(), buffer->size, state) {}

    Parcel::Parcel(u64 address, u64 size, const DeviceState &state) : state(state) {
        state.thisProcess->ReadMemory(&header, address, sizeof(ParcelHeader));
        if (size < (sizeof(ParcelHeader) + header.dataSize + header.objectsSize))
            throw exception("The size of the parcel according to the header exceeds the specified size");
        data.resize(header.dataSize);
        state.thisProcess->ReadMemory(data.data(), address + header.dataOffset, header.dataSize);
        objects.resize(header.objectsSize);
        state.thisProcess->ReadMemory(objects.data(), address + header.objectsOffset, header.objectsSize);
    }

    Parcel::Parcel(const DeviceState &state) : state(state) {}

    u64 Parcel::WriteParcel(kernel::ipc::BufferDescriptorABW *buffer, pid_t process) {
        return WriteParcel(buffer->Address(), buffer->Size(), process);
    }

    u64 Parcel::WriteParcel(kernel::ipc::BufferDescriptorC *buffer, pid_t process) {
        return WriteParcel(buffer->address, buffer->size, process);
    }

    u64 Parcel::WriteParcel(u64 address, u64 maxSize, pid_t process) {
        header.dataSize = static_cast<u32>(data.size());
        header.dataOffset = sizeof(ParcelHeader);
        header.objectsSize = static_cast<u32>(objects.size());
        header.objectsOffset = sizeof(ParcelHeader) + data.size();
        u64 totalSize = sizeof(ParcelHeader) + header.dataSize + header.objectsSize;
        if (maxSize < totalSize)
            throw exception("The size of the parcel exceeds maxSize");
        if (process) {
            auto &object = state.os->processMap.at(process);
            object->WriteMemory(header, address);
            object->WriteMemory(data.data(), address + header.dataOffset, data.size());
            object->WriteMemory(objects.data(), address + header.objectsOffset, objects.size());
        } else {
            state.thisProcess->WriteMemory(header, address);
            state.thisProcess->WriteMemory(data.data(), address + header.dataOffset, data.size());
            state.thisProcess->WriteMemory(objects.data(), address + header.objectsOffset, objects.size());
        }
        return totalSize;
    }
}
