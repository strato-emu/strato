// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/serviceman.h>
#include "IRootService.h"
#include "IApplicationDisplayService.h"
#include "results.h"

namespace skyline::service::visrv {
    IRootService::IRootService(const DeviceState &state, ServiceManager &manager, PrivilegeLevel level) : level(level), BaseService(state, manager) {}

    Result IRootService::GetDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto requestedPrivileges{request.Pop<u32>()}; //!< A boolean indicating if the returned service should have higher privileges or not
        if (requestedPrivileges && level < PrivilegeLevel::System)
            return result::IllegalOperation;
        manager.RegisterService(SRVREG(IApplicationDisplayService, level), session, response);
        return {};
    }
}
