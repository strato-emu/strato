#include "nvhost_ctrl.h"

namespace skyline::gpu::device {
    NvHostCtrl::NvHostCtrl(const DeviceState &state) : NvDevice(state, NvDeviceType::nvhost_ctrl, {}) {}
}
