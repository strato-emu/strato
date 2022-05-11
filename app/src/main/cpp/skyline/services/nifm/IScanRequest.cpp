// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IScanRequest.h"

namespace skyline::service::nifm {
    IScanRequest::IScanRequest(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}