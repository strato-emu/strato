// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IFileSystem.h"

namespace skyline::service::fssrv {
    IFileSystem::IFileSystem(const FsType type, const DeviceState &state, ServiceManager &manager) : type(type), BaseService(state, manager, Service::fssrv_IFileSystem, "fssrv:IFileSystem", {}) {}
}
