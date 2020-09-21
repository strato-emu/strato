// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/am/controller/IApplicationFunctions.h>
#include "IApplicationProxy.h"

namespace skyline::service::am {
    IApplicationProxy::IApplicationProxy(const DeviceState &state, ServiceManager &manager) : BaseProxy(state, manager) {}

    Result IApplicationProxy::GetApplicationFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IApplicationFunctions), session, response);
        return {};
    }
}
