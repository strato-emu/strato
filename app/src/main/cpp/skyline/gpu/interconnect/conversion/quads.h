// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/base.h>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace skyline::gpu::interconnect::conversion::quads {
    constexpr u32 EmittedIndexCount{6}; //!< The number of indices needed to draw a quad with two triangles
    constexpr u32 QuadVertexCount{4}; //!< The amount of vertices a quad is composed of

    /**
     * @return The amount of indices emitted converting a buffer with the supplied element count
     */
    constexpr u32 GetIndexCount(u32 count) {
        return (count * EmittedIndexCount) / QuadVertexCount;
    }

    /**
     * @return The minimum size (in bytes) required to store the quad index buffer of the given type after conversion
     * @param indexSize The size of an element in the index buffer in bytes
     */
    constexpr size_t GetRequiredBufferSize(u32 count, size_t indexSize) {
        return GetIndexCount(count) * indexSize;
    }

    /**
     * @brief Create an index buffer that repeats quad vertices to generate a triangle list
     * @note The size of the supplied buffer should be at least the size returned by GetRequiredBufferSize()
     */
    void GenerateQuadListConversionBuffer(u32 *dest, u32 vertexCount);

    /**
     * @brief Create an index buffer that repeats quad vertices from the source buffer to generate a triangle list
     * @note The size of the destination buffer should be at least the size returned by GetRequiredBufferSize()
     */
    void GenerateIndexedQuadConversionBuffer(u8 *dest, u8 *source, u32 indexCount, vk::IndexType type);
}
