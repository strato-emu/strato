// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IManagerForApplication.h"

namespace skyline::service::account {
    IManagerForApplication::IManagerForApplication(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IManagerForApplication::CheckAvailability(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(false);
        return {};
    }
}
