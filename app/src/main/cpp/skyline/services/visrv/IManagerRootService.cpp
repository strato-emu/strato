// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IManagerRootService.h"
#include "IApplicationDisplayService.h"

namespace skyline::service::visrv {
    IManagerRootService::IManagerRootService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, {
        {0x2, SFUNC(IManagerRootService::GetDisplayService)}
    }) {}

    Result IManagerRootService::GetDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IApplicationDisplayService), session, response);
        return {};
    }
}
