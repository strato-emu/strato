// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IParentalControlService.h"

namespace skyline::service::pctl {
    IParentalControlService::IParentalControlService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IParentalControlService::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
