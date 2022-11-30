// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IStaticService.h"
#include "IDatabaseService.h"

namespace skyline::service::mii {
    IStaticService::IStaticService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IStaticService::GetDatabaseService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IDatabaseService), session, response);
        return {};
    }
}
