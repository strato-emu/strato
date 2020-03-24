#include "nvhost_ctrl.h"

namespace skyline::service::nvdrv::device {
    NvHostCtrl::NvHostCtrl(const DeviceState &state) : NvDevice(state, NvDeviceType::nvhost_ctrl, {}) {}
}
