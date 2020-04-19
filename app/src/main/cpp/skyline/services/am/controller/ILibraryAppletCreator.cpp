// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ILibraryAppletCreator.h"

namespace skyline::service::am {
    ILibraryAppletCreator::ILibraryAppletCreator(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::am_ILibraryAppletCreator, "am:ILibraryAppletCreator", {
    }) {}
}
