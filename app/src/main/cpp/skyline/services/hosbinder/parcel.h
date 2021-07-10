// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/ipc.h>

namespace skyline::service::hosbinder {
    /**
     * @brief This allows easy access and efficient serialization of an Android Parcel object
     * @url https://switchbrew.org/wiki/Display_services#Parcel
     */
    class Parcel {
      private:
        struct ParcelHeader {
            u32 dataSize;
            u32 dataOffset;
            u32 objectsSize;
            u32 objectsOffset;
        } header{};
        static_assert(sizeof(ParcelHeader) == 0x10);

        const DeviceState &state;

      public:
        std::vector<u8> data;
        std::vector<u8> objects;
        size_t dataOffset{}; //!< The offset of the data read from the parcel

        /**
         * @brief This constructor fills in the Parcel object with data from a IPC buffer
         * @param buffer The buffer that contains the parcel
         * @param hasToken If the parcel starts with a token, it's skipped if this flag is true
         */
        Parcel(span<u8> buffer, const DeviceState &state, bool hasToken = false);

        /**
         * @brief This constructor is used to create an empty parcel then write to a process
         */
        Parcel(const DeviceState &state);

        /**
         * @return A reference to an item from the top of data
         */
        template<typename ValueType>
        ValueType &Pop() {
            ValueType &value{*reinterpret_cast<ValueType *>(data.data() + dataOffset)};
            dataOffset += sizeof(ValueType);
            return value;
        }

        /**
         * @return A pointer to an optional flattenable from the top of data, nullptr will be returned if the object doesn't exist
         */
        template<typename ValueType>
        ValueType *PopOptionalFlattenable() {
            bool hasObject{Pop<u32>() != 0};
            if (hasObject) {
                auto size{Pop<u64>()};
                if (size != sizeof(ValueType))
                    throw exception("Popping flattenable of size 0x{:X} with type size 0x{:X}", size, sizeof(ValueType));
                return &Pop<ValueType>();
            } else {
                return nullptr;
            }
        }

        template<typename ValueType>
        void Push(const ValueType &value) {
            auto offset{data.size()};
            data.resize(offset + sizeof(ValueType));
            std::memcpy(data.data() + offset, &value, sizeof(ValueType));
        }

        /**
         * @brief Writes a 32-bit boolean flag denoting if an object exists alongside the object if it exists
         */
        template<typename ObjectType>
        void PushOptionalFlattenable(ObjectType *pointer) {
            Push<u32>(pointer != nullptr);
            if (pointer) {
                Push<u32>(sizeof(ObjectType)); // Object Size
                Push<u32>(0); // FD Count
                Push(*pointer);
            }
        }

        template<typename ObjectType>
        void PushOptionalFlattenable(std::optional<ObjectType> object) {
            Push<u32>(object.has_value());
            if (object) {
                Push<u32>(sizeof(ObjectType));
                Push<u32>(0);
                Push(*object);
            }
        }

        template<typename ObjectType>
        void PushObject(const ObjectType &object) {
            auto offset{objects.size()};
            objects.resize(offset + sizeof(ObjectType));
            std::memcpy(objects.data() + offset, &object, sizeof(ObjectType));
        }

        /**
         * @param buffer The buffer to write the flattened Parcel into
         * @return The total size of the Parcel
         */
        u64 WriteParcel(span<u8> buffer);
    };
}
