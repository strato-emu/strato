// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief IStorage is used to open an IStorageAccessor to access a region of memory
     * @url https://switchbrew.org/wiki/Applet_Manager_services#IStorage
     */
    class IStorage : public BaseService, public std::enable_shared_from_this<IStorage> {
      private:
        size_t offset{}; //!< The current offset within the content for pushing data

      public:
        std::vector<u8> content; //!< The container for this IStorage's contents

        IStorage(const DeviceState &state, ServiceManager &manager, size_t size);

        /**
         * @brief Returns an IStorageAccessor that can read and write data to an IStorage
         */
        Result Open(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Writes an object to the storage
         */
        template<typename ValueType>
        void Push(const ValueType &value) {
            if (offset + sizeof(ValueType) > content.size())
                throw exception("The supplied value cannot fit into the IStorage");

            std::memcpy(content.data() + offset, reinterpret_cast<const u8 *>(&value), sizeof(ValueType));
            offset += sizeof(ValueType);
        }

        SERVICE_DECL(
            SFUNC(0x0, IStorage, Open)
        )
    };
}
