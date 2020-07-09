// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ISslService.h"

namespace skyline::service::ssl {
    ISslService::ISslService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::ssl_ISslService, "ssl:ISslService", {
        {0x5, SFUNC(ISslService::SetInterfaceVersion)}
    }) {}

    void ISslService::SetInterfaceVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}
}
