// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ISession.h"
#include "IManager.h"

namespace skyline::service::apm {
    IManager::IManager(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IManager::OpenSession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISession), session, response);
        return {};
    }

    Result IManager::IsCpuOverclockEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(false);
        return {};
    }
}
