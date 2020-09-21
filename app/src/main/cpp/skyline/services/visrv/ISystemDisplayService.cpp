// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ISystemDisplayService.h"

namespace skyline::service::visrv {
    ISystemDisplayService::ISystemDisplayService(const DeviceState &state, ServiceManager &manager) : IDisplayService(state, manager) {}

    Result ISystemDisplayService::SetLayerZ(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
