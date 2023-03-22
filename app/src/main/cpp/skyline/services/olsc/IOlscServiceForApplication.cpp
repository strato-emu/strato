// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IOlscServiceForApplication.h"

namespace skyline::service::olsc {
    IOlscServiceForApplication::IOlscServiceForApplication(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}
