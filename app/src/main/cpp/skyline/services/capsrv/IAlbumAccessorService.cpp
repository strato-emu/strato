// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IAlbumAccessorService.h"

namespace skyline::service::capsrv {
    IAlbumAccessorService::IAlbumAccessorService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}