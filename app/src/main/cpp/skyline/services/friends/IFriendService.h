// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::friends {
    /**
     * @brief IFriendService is used by applications to access information about a user's friends
     * @url https://switchbrew.org/wiki/Friend_services#IFriendService
     */
    class IFriendService : public BaseService {
      public:
        IFriendService(const DeviceState &state, ServiceManager &manager);

        Result GetFriendList(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x2775, IFriendService, GetFriendList)
        )
    };
}
