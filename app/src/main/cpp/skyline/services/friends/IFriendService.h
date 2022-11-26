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

        Result GetBlockedUserListIds(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result DeclareOpenOnlinePlaySession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result DeclareCloseOnlinePlaySession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result UpdateUserPresence(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetPlayHistoryRegistrationKey(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x2775, IFriendService, GetFriendList),
            SFUNC(0x28A0, IFriendService, GetBlockedUserListIds),
            SFUNC(0x2968, IFriendService, DeclareOpenOnlinePlaySession),
            SFUNC(0x2969, IFriendService, DeclareCloseOnlinePlaySession),
            SFUNC(0x2972, IFriendService, UpdateUserPresence),
            SFUNC(0x29CC, IFriendService, GetPlayHistoryRegistrationKey)
        )
    };
}
