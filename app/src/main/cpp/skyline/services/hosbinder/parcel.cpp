// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "parcel.h"

namespace skyline::service::hosbinder {
    Parcel::Parcel(span<u8> buffer, const DeviceState &state, bool hasToken) : state(state) {
        header = buffer.as<ParcelHeader>();

        if (buffer.size() < (sizeof(ParcelHeader) + header.dataSize + header.objectsSize))
            throw exception("The size of the parcel according to the header exceeds the specified size");

        constexpr u8 tokenLength{0x50}; // The length of the token on BufferQueue parcels

        data.resize(header.dataSize - (hasToken ? tokenLength : 0));
        std::memcpy(data.data(), buffer.data() + header.dataOffset + (hasToken ? tokenLength : 0), header.dataSize - (hasToken ? tokenLength : 0));

        objects.resize(header.objectsSize);
        std::memcpy(objects.data(), buffer.data() + header.objectsOffset, header.objectsSize);
    }

    Parcel::Parcel(const DeviceState &state) : state(state) {}

    u64 Parcel::WriteParcel(span<u8> buffer) {
        header.dataSize = static_cast<u32>(data.size());
        header.dataOffset = sizeof(ParcelHeader);

        header.objectsSize = static_cast<u32>(objects.size());
        header.objectsOffset = sizeof(ParcelHeader) + data.size();

        auto totalSize{sizeof(ParcelHeader) + header.dataSize + header.objectsSize};

        if (buffer.size() < totalSize)
            throw exception("The size of the parcel exceeds maxSize");

        buffer.as<ParcelHeader>() = header;
        std::memcpy(buffer.data() + header.dataOffset, data.data(), data.size());
        std::memcpy(buffer.data() + header.objectsOffset, objects.data(), objects.size());

        return totalSize;
    }
}
