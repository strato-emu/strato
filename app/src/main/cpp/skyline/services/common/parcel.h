// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <kernel/ipc.h>

namespace skyline::service {
    /**
     * @brief This class encapsulates a Parcel object (https://switchbrew.org/wiki/Display_services#Parcel)
     */
    class Parcel {
      private:
        /**
         * @brief The header of an Android Parcel structure
         */
        struct ParcelHeader {
            u32 dataSize;
            u32 dataOffset;
            u32 objectsSize;
            u32 objectsOffset;
        } header{};
        static_assert(sizeof(ParcelHeader) == 0x10);

        const DeviceState &state; //!< The state of the device

      public:
        std::vector<u8> data; //!< A vector filled with data in the parcel
        std::vector<u8> objects; //!< A vector filled with objects in the parcel
        size_t dataOffset{}; //!< This is the offset of the data read from the parcel

        /**
         * @brief This constructor fills in the Parcel object with data from a IPC buffer
         * @param buffer The buffer that contains the parcel
         * @param state The state of the device
         * @param hasToken If the parcel starts with a token, it is skipped if this flag is true
         */
        Parcel(span<u8> buffer, const DeviceState &state, bool hasToken = false);

        /**
         * @brief This constructor is used to create an empty parcel then write to a process
         * @param state The state of the device
         */
        Parcel(const DeviceState &state);

        /**
         * @return A reference to an item from the top of data
         */
        template<typename ValueType>
        inline ValueType &Pop() {
            ValueType &value = *reinterpret_cast<ValueType *>(data.data() + dataOffset);
            dataOffset += sizeof(ValueType);
            return value;
        }

        /**
         * @brief Writes some data to the Parcel
         * @tparam ValueType The type of the object to write
         * @param value The object to be written
         */
        template<typename ValueType>
        void Push(const ValueType &value) {
            data.reserve(data.size() + sizeof(ValueType));
            auto item = reinterpret_cast<const u8 *>(&value);
            for (size_t index{}; sizeof(ValueType) > index; index++) {
                data.push_back(*item);
                item++;
            }
        }

        /**
         * @brief Writes an object to the Parcel
         * @tparam ValueType The type of the object to write
         * @param value The object to be written
         */
        template<typename ValueType>
        void PushObject(const ValueType &value) {
            objects.reserve(objects.size() + sizeof(ValueType));
            auto item = reinterpret_cast<const u8 *>(&value);
            for (size_t index{}; sizeof(ValueType) > index; index++) {
                objects.push_back(*item);
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
