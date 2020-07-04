// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IAccountServiceForApplication.h"

namespace skyline::service::account {
    IAccountServiceForApplication::IAccountServiceForApplication(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::account_IAccountServiceForApplication, "account:IAccountServiceForApplication", {
        {0x64, SFUNC(IAccountServiceForApplication::InitializeApplicationInfoV0)}
    }) {}

    void IAccountServiceForApplication::InitializeApplicationInfoV0(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}
}
