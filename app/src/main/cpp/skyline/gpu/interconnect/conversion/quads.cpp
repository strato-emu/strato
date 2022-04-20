// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "quads.h"

namespace skyline::gpu::interconnect::conversion::quads {
    void GenerateQuadListConversionBuffer(u32 *dest, u32 vertexCount) {
        #pragma clang loop vectorize(enable) interleave(enable) unroll(enable)
        for (u32 i{}; i < vertexCount; i += 4) {
            // Given a quad ABCD, we want to generate triangles ABC & CDA
            // Triangle ABC
            *(dest++) = i + 0;
            *(dest++) = i + 1;
            *(dest++) = i + 2;

            // Triangle CDA
            *(dest++) = i + 2;
            *(dest++) = i + 3;
            *(dest++) = i + 0;
        }
    }
}
