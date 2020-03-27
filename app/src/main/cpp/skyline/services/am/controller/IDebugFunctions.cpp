// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IDebugFunctions.h"

namespace skyline::service::am {
    IDebugFunctions::IDebugFunctions(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::am_IDebugFunctions, "am:IDebugFunctions", {
    }) {}
}
