// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IParentalControlServiceFactory.h"

namespace skyline::service::pctl {
    IParentalControlServiceFactory::IParentalControlServiceFactory(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::pctl_IParentalControlServiceFactory, "pctl:IParentalControlServiceFactory", {
    }) {}
}
