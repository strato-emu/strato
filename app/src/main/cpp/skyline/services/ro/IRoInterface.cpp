// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IRoInterface.h"

namespace skyline::service::ro {
    IRoInterface::IRoInterface(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}
