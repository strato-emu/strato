// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IWindowController.h"

namespace skyline::service::am {
    IWindowController::IWindowController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, {
        {0x1, SFUNC(IWindowController::GetAppletResourceUserId)},
        {0xA, SFUNC(IWindowController::AcquireForegroundRights)}
    }) {}

    void IWindowController::GetAppletResourceUserId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(static_cast<u64>(state.process->pid));
    }

    void IWindowController::AcquireForegroundRights(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}
}
