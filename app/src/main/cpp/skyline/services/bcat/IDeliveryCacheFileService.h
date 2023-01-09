// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::bcat {
    /**
     * @brief IDeliveryCacheFileService is used to access BCAT files
     * @url https://switchbrew.org/wiki/BCAT_services#IDeliveryCacheFileService
     */
    class IDeliveryCacheFileService : public BaseService {
      public:
        IDeliveryCacheFileService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Given a DirectoryName and a FileName, opens the desired file
         * @url https://switchbrew.org/wiki/BCAT_services#IDeliveryCacheFileService
         */
        Result Open (type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the size (u64) of the file
         * @url https://switchbrew.org/wiki/BCAT_services#IDeliveryCacheFileService
         */
        Result GetSize (type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

      SERVICE_DECL(
        SFUNC(0x0, IDeliveryCacheFileService, Open),
        SFUNC(0x2, IDeliveryCacheFileService, GetSize)
      )
    };
}
