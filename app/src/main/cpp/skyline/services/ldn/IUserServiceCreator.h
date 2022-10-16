// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::ldn {
    /**
     * @brief IUserServiceCreator or ldn:u is used by applications to open a channel to manage LDN sessions
     * @url https://switchbrew.org/wiki/LDN_services#CreateUserLocalCommunicationService
     */
    class IUserServiceCreator : public BaseService {
      public:
        IUserServiceCreator(const DeviceState &state, ServiceManager &manager);

        Result CreateUserLocalCommunicationService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IUserServiceCreator, CreateUserLocalCommunicationService)
        )
    };
}
