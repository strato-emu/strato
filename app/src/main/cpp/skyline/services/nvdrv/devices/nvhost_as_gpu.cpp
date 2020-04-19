// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nvhost_as_gpu.h"

namespace skyline::service::nvdrv::device {
    NvHostAsGpu::NvHostAsGpu(const DeviceState &state) : NvDevice(state, NvDeviceType::nvhost_as_gpu, {}) {}
}
