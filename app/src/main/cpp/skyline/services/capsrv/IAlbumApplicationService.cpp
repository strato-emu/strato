// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IAlbumApplicationService.h"

namespace skyline::service::capsrv {
    IAlbumApplicationService::IAlbumApplicationService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}