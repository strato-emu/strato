// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ISslContext.h"

namespace skyline::service::ssl {
    ISslContext::ISslContext(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::ssl_ISslContext, "ssl:ISslContext", {
    }) {}
}
