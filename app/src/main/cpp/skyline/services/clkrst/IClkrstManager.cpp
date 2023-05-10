// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IClkrstSession.h"
#include "IClkrstManager.h"

namespace skyline::service::clkrst {
    IClkrstManager::IClkrstManager(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IClkrstManager::OpenSession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IClkrstSession), session, response);
        return {};
    }
}
