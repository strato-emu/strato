// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IManagerForApplication.h"
#include "IAccountServiceForApplication.h"

namespace skyline::service::account {
    IAccountServiceForApplication::IAccountServiceForApplication(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::account_IAccountServiceForApplication, "account:IAccountServiceForApplication", {
        {0x1, SFUNC(IAccountServiceForApplication::GetUserExistence)},
        {0x4, SFUNC(IAccountServiceForApplication::GetLastOpenedUser)},
        {0x64, SFUNC(IAccountServiceForApplication::InitializeApplicationInfoV0)},
        {0x65, SFUNC(IAccountServiceForApplication::GetBaasAccountManagerForApplication)}
    }) {}

    void IAccountServiceForApplication::GetUserExistence(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id = request.Pop<UserId>();

        response.Push<u32>(id == constant::DefaultUserId);
    }

    void IAccountServiceForApplication::GetLastOpenedUser(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(constant::DefaultUserId);
    }

    void IAccountServiceForApplication::InitializeApplicationInfoV0(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void IAccountServiceForApplication::GetBaasAccountManagerForApplication(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IManagerForApplication), session, response);
    }
}
