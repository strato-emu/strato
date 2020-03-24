#include "nvhost_as_gpu.h"

namespace skyline::service::nvdrv::device {
    NvHostAsGpu::NvHostAsGpu(const DeviceState &state) : NvDevice(state, NvDeviceType::nvhost_as_gpu, {}) {}
}
