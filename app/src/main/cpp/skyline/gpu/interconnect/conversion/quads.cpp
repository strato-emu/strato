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

    template<typename S>
    static void GenerateQuadIndexConversionBufferImpl(S *__restrict__ dest, S *__restrict__ source, u32 indexCount) {
        #pragma clang loop vectorize(enable) interleave(enable) unroll(enable)
        for (size_t i{}; i < indexCount; i += 4, source += 4) {
            // Given a quad ABCD, we want to generate triangles ABC & CDA
            // Triangle ABC
            *(dest++) = *(source + 0);
            *(dest++) = *(source + 1);
            *(dest++) = *(source + 2);

            // Triangle CDA
            *(dest++) = *(source + 2);
            *(dest++) = *(source + 3);
            *(dest++) = *(source + 0);
        }
    }

    void GenerateIndexedQuadConversionBuffer(u8 *dest, u8 *source, u32 indexCount, vk::IndexType type) {
        switch (type) {
            case vk::IndexType::eUint32:
                GenerateQuadIndexConversionBufferImpl(reinterpret_cast<u32 *>(dest), reinterpret_cast<u32 *>(source), indexCount);
                break;
            case vk::IndexType::eUint16:
                GenerateQuadIndexConversionBufferImpl(reinterpret_cast<u16 *>(dest), reinterpret_cast<u16 *>(source), indexCount);
                break;
            case vk::IndexType::eUint8EXT:
                GenerateQuadIndexConversionBufferImpl(dest, source, indexCount);
                break;
            default:
                break;
        }
    }
}
