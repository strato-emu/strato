// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "nvdevice.h"

namespace skyline::service::nvdrv::device {
    /**
     * @brief NvHostAsGpu (/dev/nvhost-as-gpu) is used to access GPU virtual address spaces (https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-as-gpu)
     */
    class NvHostAsGpu : public NvDevice {
      public:
        NvHostAsGpu(const DeviceState &state);
    };
}
