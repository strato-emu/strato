// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "nvdevice.h"

namespace skyline::service::nvdrv::device {
    /**
     * @brief NvHostCtrl (/dev/nvhost-ctrl) is used for GPU synchronization (https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-ctrl)
     */
    class NvHostCtrl : public NvDevice {
      public:
        NvHostCtrl(const DeviceState &state);
    };
}
