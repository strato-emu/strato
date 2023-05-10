// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ISession.h"

namespace skyline::service::ts {
    ISession::ISession(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}
