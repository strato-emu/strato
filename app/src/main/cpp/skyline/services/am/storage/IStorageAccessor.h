// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    class IStorage;

    /**
     * @brief IStorageAccessor is used read and write to an IStorage (https://switchbrew.org/wiki/Applet_Manager_services#IStorageAccessor)
     */
    class IStorageAccessor : public BaseService {
      private:
        std::shared_ptr<IStorage> parent; //!< The parent IStorage of the accessor

      public:
        IStorageAccessor(const DeviceState &state, ServiceManager &manager, std::shared_ptr<IStorage> parent);

        /**
         * @brief This returns the size of the storage in bytes
         */
        void GetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a buffer containing the contents of the storage at the specified offset
         */
        void Read(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
