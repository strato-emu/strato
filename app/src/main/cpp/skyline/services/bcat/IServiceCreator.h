// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>
#include "IBcatService.h"
#include "IDeliveryCacheStorageService.h"

namespace skyline::service::bcat {
    /**
     * @brief IServiceCreator is used to create per-process/per-title service instances for BCAT
     * @url https://switchbrew.org/wiki/BCAT_services#bcat:a.2C_bcat:m.2C_bcat:u.2C_bcat:s
     */
    class IServiceCreator : public BaseService {
      public:
        IServiceCreator(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Takes an input u64 ProcessId, returns an #IBcatService
         * @url https://switchbrew.org/wiki/BCAT_services#bcat:a.2C_bcat:m.2C_bcat:u.2C_bcat:s
         */
        Result CreateBcatService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Takes an input u64 ProcessId, returns an #IDeliveryCacheStorageService
         * @url https://switchbrew.org/wiki/BCAT_services#bcat:a.2C_bcat:m.2C_bcat:u.2C_bcat:s
         */
        Result CreateDeliveryCacheStorageService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IServiceCreator, CreateBcatService),
            SFUNC(0x1, IServiceCreator, CreateDeliveryCacheStorageService)
        )
    };
}
