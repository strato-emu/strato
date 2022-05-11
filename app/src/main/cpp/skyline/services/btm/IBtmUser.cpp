// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IBtmUser.h"

namespace skyline::service::btm {
    IBtmUser::IBtmUser(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IBtmUser::GetCore(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IBtmUserCore), session, response);
        return {};
    }
}
