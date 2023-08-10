// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IBcatService.h"
#include "IDeliveryCacheProgressService.h"

namespace skyline::service::bcat {
    IBcatService::IBcatService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IBcatService::RequestSyncDeliveryCache(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IDeliveryCacheProgressService), session, response);
        return {};
    }
}
