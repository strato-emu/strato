// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IDeliveryCacheFileService.h"

namespace skyline::service::bcat {
    IDeliveryCacheFileService::IDeliveryCacheFileService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IDeliveryCacheFileService::Open(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        const auto dir_name{request.PopString(0x20)};
        const auto file_name{request.PopString(0x20)};

        Logger::Debug("Directory name = {}, File name = {}", dir_name, file_name);
        return {};
    }

    Result IDeliveryCacheFileService::GetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(static_cast<u64>(0));
        return {};
    }
}
