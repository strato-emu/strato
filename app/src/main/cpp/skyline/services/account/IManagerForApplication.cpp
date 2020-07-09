// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IManagerForApplication.h"

namespace skyline::service::account {
    IManagerForApplication::IManagerForApplication(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::account_IManagerForApplication, "account:IManagerForApplication", {
    }) {}
}
