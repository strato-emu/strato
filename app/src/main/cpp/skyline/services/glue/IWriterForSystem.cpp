// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/serviceman.h>
#include "IWriterForSystem.h"
#include "IContextRegistrar.h"

namespace skyline::service::glue {
    IWriterForSystem::IWriterForSystem(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IWriterForSystem::CreateContextRegistrar(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<IContextRegistrar>(state, manager), session, response);
        return {};
    }
}
