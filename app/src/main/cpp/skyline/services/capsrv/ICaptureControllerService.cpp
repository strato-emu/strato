// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ICaptureControllerService.h"

namespace skyline::service::capsrv {
    ICaptureControllerService::ICaptureControllerService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}