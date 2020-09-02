// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IUser.h"
#include "IUserManager.h"

namespace skyline::service::nfp {
    IUser::IUser(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, {
        {0x0, SFUNC(IUser::Initialize)}
    }) {}

    void IUser::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}
}
