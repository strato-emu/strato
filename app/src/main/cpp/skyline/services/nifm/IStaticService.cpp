// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IGeneralService.h"
#include "IStaticService.h"

namespace skyline::service::nifm {
    IStaticService::IStaticService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IStaticService::CreateGeneralService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IGeneralService), session, response);
        return {};
    }
}
