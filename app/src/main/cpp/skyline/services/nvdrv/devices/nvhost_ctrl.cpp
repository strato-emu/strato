// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nvhost_ctrl.h"

namespace skyline::service::nvdrv::device {
    NvHostCtrl::NvHostCtrl(const DeviceState &state) : NvDevice(state, NvDeviceType::nvhost_ctrl, {
        {0x001F, NFUNC(NvHostCtrl::EventRegister)},
    }) {}

    void NvHostCtrl::EventRegister(IoctlData &buffer) {}
}
