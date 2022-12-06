// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IManagerForApplication.h"
#include "IAccountServiceForApplication.h"

namespace skyline::service::account {
    IManagerForApplication::IManagerForApplication(const DeviceState &state, ServiceManager &manager, std::vector<UserId> &openedUsers) : BaseService(state, manager) {
        this->openedUsers = std::make_shared<std::vector<UserId>>(openedUsers);
    }

    Result IManagerForApplication::CheckAvailability(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(false);
        return {};
    }

    Result IManagerForApplication::GetAccountId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(constant::DefaultUserId);
        return {};
    }

    Result IManagerForApplication::StoreOpenContext(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        openedUsers->clear();
        openedUsers->push_back(constant::DefaultUserId);
        return {};
    }
}
