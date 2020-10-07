// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IWindowController.h"

namespace skyline::service::am {
    IWindowController::IWindowController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IWindowController::GetAppletResourceUserId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(static_cast<u64>(0));
        return {};
    }

    Result IWindowController::AcquireForegroundRights(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
