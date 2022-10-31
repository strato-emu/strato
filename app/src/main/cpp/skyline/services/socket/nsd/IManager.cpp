// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IManager.h"

namespace skyline::service::socket {
    IManager::IManager(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}
