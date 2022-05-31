// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IShopServiceAsync.h"
#include "IShopServiceAccessor.h"

namespace skyline::service::nim {
    IShopServiceAccessor::IShopServiceAccessor(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IShopServiceAccessor::CreateAsyncInterface(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IShopServiceAsync), session, response);
        return {};
    }
}
