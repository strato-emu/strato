// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/ipc.h>

namespace skyline::service {
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
        inline ValueType &Pop() {
            ValueType &value{*reinterpret_cast<ValueType *>(data.data() + dataOffset)};
            dataOffset += sizeof(ValueType);
            return value;
        }

        /**
         * @brief Writes a value to the Parcel
         */
        template<typename ValueType>
        void Push(const ValueType &value) {
            data.reserve(data.size() + sizeof(ValueType));
            auto item{reinterpret_cast<const u8 *>(&value)};
            for (size_t index{}; sizeof(ValueType) > index; index++) {
                data.push_back(*item);
                item++;
            }
        }

        /**
         * @brief Writes an object to the Parcel
         */
        template<typename ObjectType>
        void PushObject(const ObjectType &object) {
            objects.reserve(objects.size() + sizeof(ObjectType));
            auto item{reinterpret_cast<const u8 *>(&object)};
            for (size_t index{}; sizeof(ObjectType) > index; index++) {
                objects.push_back(*object);
                item++;
            }
        }

        /**
         * @brief Writes the Parcel object out
         * @param buffer The buffer to write the Parcel object to
         * @return The total size of the message
         */
        u64 WriteParcel(span<u8> buffer);
    };
}
