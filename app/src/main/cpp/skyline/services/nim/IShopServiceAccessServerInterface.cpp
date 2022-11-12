// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IShopServiceAccessServer.h"
#include "IShopServiceAccessServerInterface.h"

namespace skyline::service::nim {
    IShopServiceAccessServerInterface::IShopServiceAccessServerInterface(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IShopServiceAccessServerInterface::CreateServerInterface(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IShopServiceAccessServer), session, response);
        return {};
    }

    Result IShopServiceAccessServerInterface::IsLargeResourceAvailable(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(false);
        return {};
    }
}
