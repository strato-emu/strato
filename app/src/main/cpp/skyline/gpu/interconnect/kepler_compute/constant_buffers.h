// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <soc/gm20b/engines/kepler_compute/qmd.h>
#include "common.h"

namespace skyline::gpu::interconnect::kepler_compute {
    using ConstantBufferSet = std::array<ConstantBuffer, QMD::ConstantBufferCount>;

    /**
     * @brief Abstracts out QMD constant buffer creation
     */
    struct ConstantBuffers {
      private:
        std::array<CachedMappedBufferView, QMD::ConstantBufferCount> cachedBuffers;

      public:
        ConstantBufferSet boundConstantBuffers{}; //!< The currently active set of constant buffers from the QMD

        void Update(InterconnectContext &ctx, const QMD &qmd);

        void MarkAllDirty();
    };
}
