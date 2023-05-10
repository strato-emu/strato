// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IClkrstSession.h"

namespace skyline::service::clkrst {
    IClkrstSession::IClkrstSession(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}
