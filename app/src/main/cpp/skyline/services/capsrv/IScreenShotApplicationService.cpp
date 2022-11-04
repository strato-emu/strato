// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IScreenShotApplicationService.h"

namespace skyline::service::capsrv {
    IScreenShotApplicationService::IScreenShotApplicationService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}