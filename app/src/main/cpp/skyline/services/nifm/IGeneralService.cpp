// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IRequest.h"
#include "IGeneralService.h"

namespace skyline::service::nifm {
    IGeneralService::IGeneralService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, {
        {0x4, SFUNC(IGeneralService::CreateRequest)}
    }) {}

    void IGeneralService::CreateRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IRequest), session, response);
    }
}
