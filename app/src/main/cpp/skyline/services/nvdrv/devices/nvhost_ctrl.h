// SPDX-License-Identifier: MPL-2.0
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

        /**
         * @brief This registers a GPU event (https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_EVENT_REGISTER)
         */
        void EventRegister(IoctlData &buffer);
    };
}
