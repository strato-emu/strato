// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <kernel/types/KProcess.h>
#include "parcel.h"

namespace skyline::service {
    Parcel::Parcel(kernel::ipc::InputBuffer &buffer, const DeviceState &state, bool hasToken) : Parcel(buffer.address, buffer.size, state, hasToken) {}

    Parcel::Parcel(u64 address, u64 size, const DeviceState &state, bool hasToken) : state(state) {
        state.process->ReadMemory(&header, address, sizeof(ParcelHeader));

        if (size < (sizeof(ParcelHeader) + header.dataSize + header.objectsSize))
            throw exception("The size of the parcel according to the header exceeds the specified size");

        constexpr auto tokenLength = 0x50; // The length of the token on BufferQueue parcels

        data.resize(header.dataSize - (hasToken ? tokenLength : 0));
        state.process->ReadMemory(data.data(), address + header.dataOffset + (hasToken ? tokenLength : 0), header.dataSize - (hasToken ? tokenLength : 0));

        objects.resize(header.objectsSize);
        state.process->ReadMemory(objects.data(), address + header.objectsOffset, header.objectsSize);
    }

    Parcel::Parcel(const DeviceState &state) : state(state) {}

    u64 Parcel::WriteParcel(kernel::ipc::OutputBuffer &buffer) {
        return WriteParcel(buffer.address, buffer.size);
    }

    u64 Parcel::WriteParcel(u64 address, u64 maxSize) {
        header.dataSize = static_cast<u32>(data.size());
        header.dataOffset = sizeof(ParcelHeader);

        header.objectsSize = static_cast<u32>(objects.size());
        header.objectsOffset = sizeof(ParcelHeader) + data.size();

        auto totalSize = sizeof(ParcelHeader) + header.dataSize + header.objectsSize;

        if (maxSize < totalSize)
            throw exception("The size of the parcel exceeds maxSize");

        state.process->WriteMemory(header, address);
        state.process->WriteMemory(data.data(), address + header.dataOffset, data.size());
        state.process->WriteMemory(objects.data(), address + header.objectsOffset, objects.size());

        return totalSize;
    }
}
