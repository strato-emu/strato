// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IDeliveryCacheStorageService.h"

namespace skyline::service::bcat {
    IDeliveryCacheStorageService::IDeliveryCacheStorageService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}
