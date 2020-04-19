// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IAudioController.h"

namespace skyline::service::am {
    IAudioController::IAudioController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::am_IAudioController, "am:IAudioController", {
    }) {}
}
