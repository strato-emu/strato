// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IResolver.h"

namespace skyline::service::socket {
    IResolver::IResolver(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}
