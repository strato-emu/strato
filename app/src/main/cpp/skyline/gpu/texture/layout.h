// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "texture.h"

namespace skyline::gpu::texture {
    /**
     * @return The size of a layer of the specified non-mipmapped block-slinear surface in bytes
     */
    size_t GetBlockLinearLayerSize(Dimensions dimensions,
                                   size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb,
                                   size_t gobBlockHeight, size_t gobBlockDepth);

    /**
     * @param isMultiLayer If the texture has more than one layer, a multi-layer texture requires alignment to a block at layer end
     * @return The size of a layer of the specified block-linear surface in bytes
     */
    size_t GetBlockLinearLayerSize(Dimensions dimensions,
                                   size_t formatBlockHeight, size_t formatBlockWidth, size_t formatBpb,
                                   size_t gobBlockHeight, size_t gobBlockDepth,
                                   size_t levelCount, bool isMultiLayer);

    /**
     * @note The target format is the format of the texture after it has been decoded, if bpb is 0, the target format is the same as the source format
     * @return A vector of metadata about every mipmapped level of the supplied block-linear surface
     */
    std::vector<MipLevelLayout> GetBlockLinearMipLayout(Dimensions dimensions,
                                                        size_t formatBlockHeight, size_t formatBlockWidth, size_t formatBpb,
                                                        size_t targetFormatBlockHeight, size_t targetFormatBlockWidth, size_t targetFormatBpb,
                                                        size_t gobBlockHeight, size_t gobBlockDepth,
                                                        size_t levelCount);

    /**
     * @brief Copies the contents of a blocklinear texture to a linear output buffer
     */
    void CopyBlockLinearToLinear(Dimensions dimensions,
                                 size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb,
                                 size_t gobBlockHeight, size_t gobBlockDepth,
                                 u8 *blockLinear, u8 *linear);

    /**
     * @brief Copies the contents of a blocklinear texture to a pitch texture
     */
    void CopyBlockLinearToPitch(Dimensions dimensions,
                                size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, u32 pitchAmount,
                                size_t gobBlockHeight, size_t gobBlockDepth,
                                u8 *blockLinear, u8 *pitch);

    /**
     * @brief Copies the contents of a part of a blocklinear texture to a pitch texture
     */
    void CopyBlockLinearToPitchSubrect(Dimensions pitchDimensions, Dimensions blockLinearDimensions,
                                       size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, u32 pitchAmount,
                                       size_t gobBlockHeight, size_t gobBlockDepth,
                                       u8 *blockLinear, u8 *pitch,
                                       u32 originX, u32 originY);

    /**
     * @brief Copies the contents of a blocklinear guest texture to a linear output buffer
     */
    void CopyBlockLinearToLinear(const GuestTexture &guest, u8 *blockLinear, u8 *linear);

    /**
     * @brief Copies the contents of a linear buffer to a blocklinear texture
     */
    void CopyLinearToBlockLinear(Dimensions dimensions,
                                size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb,
                                size_t gobBlockHeight, size_t gobBlockDepth,
                                u8 *linear, u8 *blockLinear);

    /**
     * @brief Copies the contents of a pitch texture to a blocklinear texture
     */
    void CopyPitchToBlockLinear(Dimensions dimensions,
                                 size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, u32 pitchAmount,
                                 size_t gobBlockHeight, size_t gobBlockDepth,
                                 u8 *pitch, u8 *blockLinear);

    /**
     * @brief Copies the contents of a linear texture to a part of a blocklinear texture
     */
    void CopyLinearToBlockLinearSubrect(Dimensions linearDimensions, Dimensions blockLinearDimensions,
                                       size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb,
                                       size_t gobBlockHeight, size_t gobBlockDepth,
                                       u8 *linear, u8 *blockLinear,
                                        u32 originX, u32 originY);

    /**
     * @brief Copies the contents of a pitch texture to a part of a blocklinear texture
     */
    void CopyPitchToBlockLinearSubrect(Dimensions pitchDimensions, Dimensions blockLinearDimensions,
                                 size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb, u32 pitchAmount,
                                 size_t gobBlockHeight, size_t gobBlockDepth,
                                 u8 *pitch, u8 *blockLinear,
                                 u32 originX, u32 originY);

    /**
     * @brief Copies the contents of a linear guest texture to a blocklinear texture
     */
    void CopyLinearToBlockLinear(const GuestTexture &guest, u8 *linear, u8 *blockLinear);

    /**
     * @brief Copies the contents of a pitch-linear guest texture to a linear output buffer
     * @note This does not support 3D textures
     */
    void CopyPitchLinearToLinear(const GuestTexture &guest, u8 *guestInput, u8 *linearOutput);

    /**
     * @brief Copies the contents of a linear buffer to a pitch-linear guest texture
     * @note This does not support 3D textures
     */
    void CopyLinearToPitchLinear(const GuestTexture &guest, u8 *linearInput, u8 *guestOutput);
}
