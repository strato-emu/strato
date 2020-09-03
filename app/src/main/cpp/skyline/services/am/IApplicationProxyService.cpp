// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "proxy/IApplicationProxy.h"
#include "IApplicationProxyService.h"

namespace skyline::service::am {
    IApplicationProxyService::IApplicationProxyService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, {
        {0x0, SFUNC(IApplicationProxyService::OpenApplicationProxy)}
    }) {}

    Result IApplicationProxyService::OpenApplicationProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IApplicationProxy), session, response);
        return {};
    }
}
