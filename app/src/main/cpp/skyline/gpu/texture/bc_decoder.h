// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <cstdint>

namespace bcn {
    /**
     * @brief Decodes a BC1 encoded image to R8G8B8A8
     */
    void DecodeBc1(const uint8_t *src, uint8_t *dst, size_t width, size_t height, bool hasAlphaChannel);

    /**
     * @brief Decodes a BC2 encoded image to R8G8B8A8
     */
    void DecodeBc2(const uint8_t *src, uint8_t *dst, size_t width, size_t height);

    /**
     * @brief Decodes a BC3 encoded image to R8G8B8A8
     */
    void DecodeBc3(const uint8_t *src, uint8_t *dst, size_t width, size_t height);

    /**
     * @brief Decodes a BC4 encoded image to R8
     */
    void DecodeBc4(const uint8_t *src, uint8_t *dst, size_t width, size_t height, bool isSigned);

    /**
     * @brief Decodes a BC5 encoded image to R8G8
     */
    void DecodeBc5(const uint8_t *src, uint8_t *dst, size_t width, size_t height, bool isSigned);

    /**
     * @brief Decodes a BC6 encoded image to R16G16B16A16
     */
    void DecodeBc6(const uint8_t *src, uint8_t *dst, size_t width, size_t height, bool isSigned);

    /**
     * @brief Decodes a BC7 encoded image to R8G8B8A8
     */
    void DecodeBc7(const uint8_t *src, uint8_t *dst, size_t width, size_t height);
}
