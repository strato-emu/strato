// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::friends {
    /**
     * @brief IServiceCreator or friend:u is used by applications to open an IFriendService instance for accessing user friend info (https://switchbrew.org/wiki/Friend_services#friend:u.2C_friend:v.2C_friend:m.2C_friend:s.2C_friend:a)
     */
    class IServiceCreator : public BaseService {
      public:
        IServiceCreator(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This opens an IFriendService that can be used by applications to access user friend info
         */
        Result CreateFriendService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This opens an INotificationService that can be used by applications to receive notifications
         */
        Result CreateNotificationService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IServiceCreator, CreateFriendService),
            SFUNC(0x1, IServiceCreator, CreateNotificationService)
        )
    };
}
