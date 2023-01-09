// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "INotificationServicesForApplication.h"

namespace skyline::service::glue {
    INotificationServicesForApplication::INotificationServicesForApplication(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}
