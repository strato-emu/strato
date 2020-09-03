// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ISystemDisplayService.h"

namespace skyline::service::visrv {
    ISystemDisplayService::ISystemDisplayService(const DeviceState &state, ServiceManager &manager) : IDisplayService(state, manager, {
        {0x89D, SFUNC(ISystemDisplayService::SetLayerZ)},
        {0x908, SFUNC(IDisplayService::CreateStrayLayer)}
    }) {}

    Result ISystemDisplayService::SetLayerZ(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
