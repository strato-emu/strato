// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IDeliveryCacheStorageService.h"
#include "IDeliveryCacheFileService.h"
#include "IDeliveryCacheDirectoryService.h"

namespace skyline::service::bcat {
    IDeliveryCacheStorageService::IDeliveryCacheStorageService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IDeliveryCacheStorageService::CreateFileService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IDeliveryCacheFileService), session, response);
        return {};
    }

    Result IDeliveryCacheStorageService::CreateDirectoryService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IDeliveryCacheDirectoryService), session, response);
        return {};
    }

    Result IDeliveryCacheStorageService::EnumerateDeliveryCacheDirectory(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(static_cast<u32>(0));
        return {};
    }
}
