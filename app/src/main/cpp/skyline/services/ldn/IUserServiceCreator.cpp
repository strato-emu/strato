// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IUserLocalCommunicationService.h"
#include "IUserServiceCreator.h"

namespace skyline::service::ldn {
    IUserServiceCreator::IUserServiceCreator(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IUserServiceCreator::CreateUserLocalCommunicationService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IUserLocalCommunicationService), session, response);
        return {};
    }
}
