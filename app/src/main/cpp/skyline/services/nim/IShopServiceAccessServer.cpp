// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IShopServiceAccessor.h"
#include "IShopServiceAccessServer.h"

namespace skyline::service::nim {
    IShopServiceAccessServer::IShopServiceAccessServer(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IShopServiceAccessServer::CreateAccessorInterface(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IShopServiceAccessor), session, response);
        return {};
    }
}
