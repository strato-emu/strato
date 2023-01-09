// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::bcat {
    /**
     * @brief IDeliveryCacheDirectoryService is used to access BCAT directories
     * @url https://switchbrew.org/wiki/BCAT_services#IDeliveryCacheDirectoryService
     */
    class IDeliveryCacheDirectoryService : public BaseService {
      public:
        IDeliveryCacheDirectoryService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Given a DirectoryName, opens that directory
         * @url https://switchbrew.org/wiki/BCAT_services#IDeliveryCacheDirectoryService
         */
        Result Open (type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the number (u32) of elements inside the directory (u64)
         * @url https://switchbrew.org/wiki/BCAT_services#IDeliveryCacheDirectoryService
         */
        Result GetCount (type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
          SFUNC(0x0, IDeliveryCacheDirectoryService, Open),
          SFUNC(0x2, IDeliveryCacheDirectoryService, GetCount)
        )
    };
}
