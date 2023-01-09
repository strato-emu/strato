// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IDeliveryCacheDirectoryService.h"

namespace skyline::service::bcat {
    IDeliveryCacheDirectoryService::IDeliveryCacheDirectoryService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IDeliveryCacheDirectoryService::Open(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        const auto dir_name{request.PopString(0x20)};

        Logger::Debug("Directory name = {}", dir_name);
        return {};
    }

    Result IDeliveryCacheDirectoryService::GetCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(static_cast<u32>(0));
        return {};
    }
}
