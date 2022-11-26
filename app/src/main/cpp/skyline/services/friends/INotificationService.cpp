// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "INotificationService.h"

namespace skyline::service::friends {
    INotificationService::INotificationService(const DeviceState &state, ServiceManager &manager)
        : notificationEvent(std::make_shared<type::KEvent>(state, false)),
          BaseService(state, manager) {}

    Result INotificationService::GetEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        KHandle handle{state.process->InsertItem(notificationEvent)};
        Logger::Debug("Friend Notification Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }

    Result INotificationService::Pop(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return result::NoNotifications;
    }
}
