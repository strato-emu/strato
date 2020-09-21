// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IFriendService.h"
#include "INotificationService.h"
#include "IServiceCreator.h"

namespace skyline::service::friends {
    IServiceCreator::IServiceCreator(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IServiceCreator::CreateFriendService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IFriendService), session, response);
        return {};
    }

    Result IServiceCreator::CreateNotificationService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(INotificationService), session, response);
        return {};
    }
}
