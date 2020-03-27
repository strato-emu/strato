// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IDisplayController.h"

namespace skyline::service::am {
    IDisplayController::IDisplayController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::am_IDisplayController, "am:IDisplayController", {
    }) {}
}
