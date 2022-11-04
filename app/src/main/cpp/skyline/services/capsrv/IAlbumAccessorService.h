// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::capsrv {
    /**
     * @url https://switchbrew.org/wiki/Capture_services#caps:a
     */
    class IAlbumAccessorService : public BaseService {
      public:
        IAlbumAccessorService(const DeviceState &state, ServiceManager &manager);
    };
}