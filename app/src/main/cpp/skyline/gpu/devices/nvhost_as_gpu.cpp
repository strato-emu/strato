#include "nvhost_as_gpu.h"

namespace skyline::gpu::device {
    NvHostAsGpu::NvHostAsGpu(const DeviceState &state) : NvDevice(state, NvDeviceType::nvhost_as_gpu, {}) {}
}
