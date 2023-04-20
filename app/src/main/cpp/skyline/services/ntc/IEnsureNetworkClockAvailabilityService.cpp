// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IEnsureNetworkClockAvailabilityService.h"
#include <common/settings.h>

namespace skyline::service::ntc {
    IEnsureNetworkClockAvailabilityService::IEnsureNetworkClockAvailabilityService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager),
        finishNotificationEvent(std::make_shared<kernel::type::KEvent>(state, false)) {}

    Result IEnsureNetworkClockAvailabilityService::StartTask(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!(*state.settings->isInternetEnabled))
            return result::NetworkTimeNotAvailable;
        finishNotificationEvent->Signal();
        return {};
    }
}
