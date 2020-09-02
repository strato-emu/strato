// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IUser.h"
#include "IUserManager.h"

namespace skyline::service::nfp {
    IUserManager::IUserManager(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, {
        {0x0, SFUNC(IUserManager::CreateUserInterface)}
    }) {}

    void IUserManager::CreateUserInterface(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IUser), session, response);
    }
}
