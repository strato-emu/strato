// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nvhost_ctrl.h"

namespace skyline::service::nvdrv::device {
    NvHostCtrl::NvHostCtrl(const DeviceState &state) : NvDevice(state, NvDeviceType::nvhost_ctrl, {}) {}
}
