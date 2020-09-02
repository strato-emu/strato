// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IAppletCommonFunctions.h"

namespace skyline::service::am {
    IAppletCommonFunctions::IAppletCommonFunctions(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, {
    }) {}
}
