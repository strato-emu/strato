// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IServiceCreator.h"

namespace skyline::service::bcat {
    IServiceCreator::IServiceCreator(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IServiceCreator::CreateBcatService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IBcatService), session, response);
        return {};
    }

    Result IServiceCreator::CreateDeliveryCacheStorageService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IDeliveryCacheStorageService), session, response);
        return {};
    }
}
