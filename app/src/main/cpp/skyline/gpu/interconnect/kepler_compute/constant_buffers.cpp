// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "constant_buffers.h"

namespace skyline::gpu::interconnect::kepler_compute {
    void ConstantBuffers::Update(InterconnectContext &ctx, const QMD &qmd) {
        for (u32 i{}; i < QMD::ConstantBufferCount; i++) {
            if (qmd.constantBufferValid & (1U << i)) {
                auto &buffer{cachedBuffers[i]};
                const auto &qmdBuffer{qmd.constantBuffer[i]};
                buffer.Update(ctx, qmdBuffer.Address(), qmdBuffer.size);
                boundConstantBuffers[i] = {*buffer};
            }
        }
    }

    void ConstantBuffers::MarkAllDirty() {
        for (auto &buffer : cachedBuffers)
            buffer.PurgeCaches();
    }
}
