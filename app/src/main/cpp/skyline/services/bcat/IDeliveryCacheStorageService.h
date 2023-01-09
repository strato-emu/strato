// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::bcat {
    /**
     * @brief IDeliveryCacheStorageService is used to create files instances for BCAT
     * @url https://switchbrew.org/wiki/BCAT_services#IDeliveryCacheStorageService
     */
    class IDeliveryCacheStorageService : public BaseService {
      public:
        IDeliveryCacheStorageService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns #IDeliveryCacheFileService
         * @url https://switchbrew.org/wiki/BCAT_services#IDeliveryCacheStorageService
         */
        Result CreateFileService (type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns #IDeliveryCacheDirectoryService
         * @url https://switchbrew.org/wiki/BCAT_services#IDeliveryCacheStorageService
         */
        Result CreateDirectoryService (type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result EnumerateDeliveryCacheDirectory (type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IDeliveryCacheStorageService, CreateFileService),
            SFUNC(0x1, IDeliveryCacheStorageService, CreateDirectoryService),
            SFUNC(0xA, IDeliveryCacheStorageService, EnumerateDeliveryCacheDirectory)
        )
    };
}
