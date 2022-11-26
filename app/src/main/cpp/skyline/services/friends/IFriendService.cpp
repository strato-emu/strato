// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IFriendService.h"

namespace skyline::service::friends {
    IFriendService::IFriendService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IFriendService::GetFriendList(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(0); // Count of friends
        return {};
    }

    Result IFriendService::GetBlockedUserListIds(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(0); // No blocked users
        return {};
    }

    Result IFriendService::DeclareOpenOnlinePlaySession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IFriendService::DeclareCloseOnlinePlaySession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IFriendService::UpdateUserPresence(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IFriendService::GetPlayHistoryRegistrationKey(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
