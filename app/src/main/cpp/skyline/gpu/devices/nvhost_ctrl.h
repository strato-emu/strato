#pragma once

#include "nvdevice.h"

namespace skyline::gpu::device {
    /**
     * @brief NvHostCtrl (/dev/nvhost-ctrl) is used for GPU synchronization (https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-ctrl)
     */
    class NvHostCtrl : public NvDevice {
      public:
        NvHostCtrl(const DeviceState &state);
    };
}
