// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::gpu::interconnect::kepler_compute {
    /**
     * @brief Packed struct of pipeline state suitable for use as a map key
     */
    struct PackedPipelineState {
        u64 shaderHash;
        std::array<u32, 3> dimensions;
        u32 localMemorySize;
        u32 sharedMemorySize;
        u32 bindlessTextureConstantBufferSlotSelect;

        bool operator==(const PackedPipelineState &) const = default;
    };
}
