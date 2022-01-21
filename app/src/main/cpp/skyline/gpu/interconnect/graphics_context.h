// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <boost/container/static_vector.hpp>
#include <range/v3/algorithm.hpp>
#include <gpu/texture/format.h>
#include <gpu/buffer_manager.h>
#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include <soc/gm20b/engines/maxwell/types.h>

#include "command_executor.h"
#include "types/tsc.h"
#include "types/tic.h"

namespace skyline::gpu::interconnect {
    namespace maxwell3d = soc::gm20b::engine::maxwell3d::type;
    namespace ShaderCompiler = ::Shader; //!< Namespace alias to avoid conflict with the `Shader` class

    /**
     * @brief Host-equivalent context for state of the Maxwell3D engine on the guest
     * @note This class is **NOT** thread-safe and should not be utilized by multiple threads concurrently
     */
    class GraphicsContext {
      private:
        GPU &gpu;
        soc::gm20b::ChannelContext &channelCtx;
        gpu::interconnect::CommandExecutor &executor;

        /**
         * @brief A host IOVA address composed of 32-bit low/high register values
         * @note This differs from maxwell3d::Address in that it is little-endian rather than big-endian ordered for the register values
         */
        union IOVA {
            u64 iova;
            struct {
                u32 low;
                u32 high;
            };

            operator u64 &() {
                return iova;
            }
        };
        static_assert(sizeof(IOVA) == sizeof(u64));

      public:
        GraphicsContext(GPU &gpu, soc::gm20b::ChannelContext &channelCtx, gpu::interconnect::CommandExecutor &executor) : gpu(gpu), channelCtx(channelCtx), executor(executor), pipelineCache(gpu.vkDevice, vk::PipelineCacheCreateInfo{}) {
            scissors.fill(DefaultScissor);

            u32 bindingIndex{};
            for (auto &vertexBuffer : vertexBuffers) {
                vertexBuffer.bindingDescription.binding = bindingIndex;
                vertexBuffer.bindingDivisorDescription.binding = bindingIndex;
                bindingIndex++;
            }

            u32 attributeIndex{};
            for (auto &vertexAttribute : vertexAttributes)
                vertexAttribute.description.location = attributeIndex++;

            for (auto &rtBlendState : commonRtBlendState)
                rtBlendState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

            for (auto &rtBlendState : independentRtBlendState)
                rtBlendState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

            if (!gpu.quirks.supportsLastProvokingVertex)
                rasterizerState.unlink<vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT>();
        }

        /* Render Targets + Render Target Control */
      private:
        struct RenderTarget {
            bool disabled{true}; //!< If this RT has been disabled and will be an unbound attachment instead
            IOVA iova;
            u32 widthBytes; //!< The width in bytes for linear textures
            GuestTexture guest;
            std::shared_ptr<TextureView> view;

            RenderTarget() {
                guest.dimensions = texture::Dimensions(1, 1, 1); // We want the depth to be 1 by default (It cannot be set by the application)
            }
        };

        std::array<RenderTarget, maxwell3d::RenderTargetCount> colorRenderTargets{}; //!< The target textures to render into as color attachments
        maxwell3d::RenderTargetControl renderTargetControl{};

        RenderTarget depthRenderTarget{}; //!< The target texture to render depth/stencil into

      public:
        void SetDepthRenderTargetEnabled(bool enabled) {
            depthRenderTarget.disabled = !enabled;
        }

        void SetRenderTargetAddressHigh(RenderTarget &renderTarget, u32 high) {
            renderTarget.iova.high = high;
            renderTarget.guest.mappings.clear();
            renderTarget.view.reset();
        }

        void SetColorRenderTargetAddressHigh(size_t index, u32 high) {
            SetRenderTargetAddressHigh(colorRenderTargets.at(index), high);
        }

        void SetDepthRenderTargetAddressHigh(u32 high) {
            SetRenderTargetAddressHigh(depthRenderTarget, high);
        }

        void SetRenderTargetAddressLow(RenderTarget &renderTarget, u32 low) {
            renderTarget.iova.low = low;
            renderTarget.guest.mappings.clear();
            renderTarget.view.reset();
        }

        void SetColorRenderTargetAddressLow(size_t index, u32 low) {
            SetRenderTargetAddressLow(colorRenderTargets.at(index), low);
        }

        void SetDepthRenderTargetAddressLow(u32 low) {
            SetRenderTargetAddressLow(depthRenderTarget, low);
        }

        void SetRenderTargetWidth(RenderTarget &renderTarget, u32 value) {
            renderTarget.widthBytes = value;
            if (renderTarget.guest.tileConfig.mode == texture::TileMode::Linear && renderTarget.guest.format)
                value /= renderTarget.guest.format->bpb; // Width is in bytes rather than format units for linear textures
            renderTarget.guest.dimensions.width = value;
            renderTarget.view.reset();
        }

        void SetColorRenderTargetWidth(size_t index, u32 value) {
            SetRenderTargetWidth(colorRenderTargets.at(index), value);
        }

        void SetDepthRenderTargetWidth(u32 value) {
            SetRenderTargetWidth(depthRenderTarget, value);
        }

        void SetRenderTargetHeight(RenderTarget &renderTarget, u32 value) {
            renderTarget.guest.dimensions.height = value;
            renderTarget.view.reset();
        }

        void SetColorRenderTargetHeight(size_t index, u32 value) {
            SetRenderTargetHeight(colorRenderTargets.at(index), value);
        }

        void SetDepthRenderTargetHeight(u32 value) {
            SetRenderTargetHeight(depthRenderTarget, value);
        }

        void SetColorRenderTargetFormat(size_t index, maxwell3d::ColorRenderTarget::Format format) {
            auto &renderTarget{colorRenderTargets.at(index)};
            renderTarget.guest.format = [&]() -> texture::Format {
                #define FORMAT_CASE(maxwellFmt, skFmt, type) \
                    case maxwell3d::ColorRenderTarget::Format::maxwellFmt ## type: \
                        return format::skFmt ## type

                #define FORMAT_SAME_CASE(fmt, type) FORMAT_CASE(fmt, fmt, type)

                #define FORMAT_INT_CASE(maxwellFmt, skFmt) \
                    FORMAT_CASE(maxwellFmt, skFmt, Sint); \
                    FORMAT_CASE(maxwellFmt, skFmt, Uint)

                #define FORMAT_SAME_INT_CASE(fmt) FORMAT_INT_CASE(fmt, fmt)

                #define FORMAT_INT_FLOAT_CASE(maxwellFmt, skFmt) \
                    FORMAT_INT_CASE(maxwellFmt, skFmt); \
                    FORMAT_CASE(maxwellFmt, skFmt, Float)

                #define FORMAT_SAME_INT_FLOAT_CASE(fmt) FORMAT_INT_FLOAT_CASE(fmt, fmt)

                #define FORMAT_NORM_CASE(maxwellFmt, skFmt) \
                    FORMAT_CASE(maxwellFmt, skFmt, Snorm); \
                    FORMAT_CASE(maxwellFmt, skFmt, Unorm)

                #define FORMAT_NORM_INT_CASE(maxwellFmt, skFmt) \
                    FORMAT_NORM_CASE(maxwellFmt, skFmt); \
                    FORMAT_INT_CASE(maxwellFmt, skFmt)

                #define FORMAT_SAME_NORM_INT_CASE(fmt) FORMAT_NORM_INT_CASE(fmt, fmt)

                #define FORMAT_NORM_INT_SRGB_CASE(maxwellFmt, skFmt) \
                    FORMAT_NORM_INT_CASE(maxwellFmt, skFmt); \
                    FORMAT_CASE(maxwellFmt, skFmt, Srgb)

                #define FORMAT_NORM_INT_FLOAT_CASE(maxwellFmt, skFmt) \
                    FORMAT_NORM_INT_CASE(maxwellFmt, skFmt); \
                    FORMAT_CASE(maxwellFmt, skFmt, Float)

                #define FORMAT_SAME_NORM_INT_FLOAT_CASE(fmt) FORMAT_NORM_INT_FLOAT_CASE(fmt, fmt)

                switch (format) {
                    case maxwell3d::ColorRenderTarget::Format::None:
                        return {};

                    FORMAT_SAME_NORM_INT_CASE(R8);
                    FORMAT_SAME_NORM_INT_FLOAT_CASE(R16);
                    FORMAT_SAME_NORM_INT_CASE(R8G8);
                    FORMAT_SAME_CASE(B5G6R5, Unorm);
                    FORMAT_SAME_CASE(B5G5R5A1, Unorm);
                    FORMAT_SAME_INT_FLOAT_CASE(R32);
                    FORMAT_SAME_NORM_INT_FLOAT_CASE(R16G16);
                    FORMAT_SAME_CASE(R8G8B8A8, Unorm);
                    FORMAT_SAME_CASE(R8G8B8A8, Srgb);
                    FORMAT_NORM_INT_SRGB_CASE(R8G8B8X8, R8G8B8A8);
                    FORMAT_SAME_CASE(B8G8R8A8, Unorm);
                    FORMAT_SAME_CASE(B8G8R8A8, Srgb);
                    FORMAT_SAME_CASE(A2R10G10B10, Unorm);
                    FORMAT_SAME_CASE(A2R10G10B10, Uint);
                    FORMAT_SAME_INT_CASE(R32G32);
                    FORMAT_SAME_CASE(R32G32, Float);
                    FORMAT_SAME_CASE(R16G16B16A16, Float);
                    FORMAT_NORM_INT_FLOAT_CASE(R16G16B16X16, R16G16B16A16);
                    FORMAT_SAME_CASE(R32G32B32A32, Float);
                    FORMAT_INT_FLOAT_CASE(R32G32B32X32, R32G32B32A32);

                    default:
                        throw exception("Cannot translate the supplied color RT format: 0x{:X}", static_cast<u32>(format));
                }

                #undef FORMAT_CASE
                #undef FORMAT_SAME_CASE
                #undef FORMAT_INT_CASE
                #undef FORMAT_SAME_INT_CASE
                #undef FORMAT_INT_FLOAT_CASE
                #undef FORMAT_SAME_INT_FLOAT_CASE
                #undef FORMAT_NORM_CASE
                #undef FORMAT_NORM_INT_CASE
                #undef FORMAT_SAME_NORM_INT_CASE
                #undef FORMAT_NORM_INT_SRGB_CASE
                #undef FORMAT_NORM_INT_FLOAT_CASE
                #undef FORMAT_SAME_NORM_INT_FLOAT_CASE
            }();

            if (renderTarget.guest.tileConfig.mode == texture::TileMode::Linear && renderTarget.guest.format)
                renderTarget.guest.dimensions.width = renderTarget.widthBytes / renderTarget.guest.format->bpb;

            renderTarget.disabled = !renderTarget.guest.format;
            renderTarget.view.reset();
        }

        void SetDepthRenderTargetFormat(maxwell3d::DepthRtFormat format) {
            depthRenderTarget.guest.format = [&]() -> texture::Format {
                using MaxwellDepthRtFormat = maxwell3d::DepthRtFormat;
                switch (format) {
                    case MaxwellDepthRtFormat::D16Unorm:
                        return format::D16Unorm;
                    case MaxwellDepthRtFormat::D32Float:
                        return format::D32Float;
                    case MaxwellDepthRtFormat::S8D24Unorm:
                        return format::S8D24Unorm;
                    default:
                        throw exception("Cannot translate the supplied depth RT format: 0x{:X}", static_cast<u32>(format));
                }
            }();

            if (depthRenderTarget.guest.tileConfig.mode == texture::TileMode::Linear && depthRenderTarget.guest.format)
                depthRenderTarget.guest.dimensions.width = depthRenderTarget.widthBytes / depthRenderTarget.guest.format->bpb;

            depthRenderTarget.view.reset();
        }

        void SetRenderTargetTileMode(RenderTarget &renderTarget, maxwell3d::RenderTargetTileMode mode) {
            auto &config{renderTarget.guest.tileConfig};
            if (mode.isLinear) {
                if (config.mode != texture::TileMode::Linear && renderTarget.guest.format) {
                    // Width is provided in bytes rather than format units for linear textures
                    renderTarget.widthBytes = renderTarget.guest.dimensions.width;
                    renderTarget.guest.dimensions.width /= renderTarget.guest.format->bpb;
                }

                config.mode = texture::TileMode::Linear;
            } else [[likely]] {
                if (config.mode == texture::TileMode::Linear && renderTarget.guest.format)
                    renderTarget.guest.dimensions.width = renderTarget.widthBytes;

                config = texture::TileConfig{
                    .mode = texture::TileMode::Block,
                    .blockHeight = static_cast<u8>(1U << mode.blockHeightLog2),
                    .blockDepth = static_cast<u8>(1U << mode.blockDepthLog2),
                };
            }
            renderTarget.view.reset();
        }

        void SetColorRenderTargetTileMode(size_t index, maxwell3d::RenderTargetTileMode mode) {
            SetRenderTargetTileMode(colorRenderTargets.at(index), mode);
        }

        void SetDepthRenderTargetTileMode(maxwell3d::RenderTargetTileMode mode) {
            SetRenderTargetTileMode(depthRenderTarget, mode);
        }

        void SetRenderTargetArrayMode(RenderTarget &renderTarget, maxwell3d::RenderTargetArrayMode mode) {
            renderTarget.guest.layerCount = mode.layerCount;
            renderTarget.view.reset();
        }

        void SetColorRenderTargetArrayMode(size_t index, maxwell3d::RenderTargetArrayMode mode) {
            if (mode.volume)
                throw exception("Color RT Array Volumes are not supported (with layer count = {})", mode.layerCount);
            SetRenderTargetArrayMode(colorRenderTargets.at(index), mode);
        }

        void SetDepthRenderTargetArrayMode(maxwell3d::RenderTargetArrayMode mode) {
            SetRenderTargetArrayMode(depthRenderTarget, mode);
        }

        void SetRenderTargetLayerStride(RenderTarget &renderTarget, u32 layerStrideLsr2) {
            renderTarget.guest.layerStride = layerStrideLsr2 << 2;
            renderTarget.view.reset();
        }

        void SetColorRenderTargetLayerStride(size_t index, u32 layerStrideLsr2) {
            SetRenderTargetLayerStride(colorRenderTargets.at(index), layerStrideLsr2);
        }

        void SetDepthRenderTargetLayerStride(u32 layerStrideLsr2) {
            SetRenderTargetLayerStride(depthRenderTarget, layerStrideLsr2);
        }

        void SetColorRenderTargetBaseLayer(size_t index, u32 baseArrayLayer) {
            auto &renderTarget{colorRenderTargets.at(index)};
            if (baseArrayLayer > std::numeric_limits<u16>::max())
                throw exception("Base array layer ({}) exceeds the range of array count ({}) (with layer count = {})", baseArrayLayer, std::numeric_limits<u16>::max(), renderTarget.guest.layerCount);

            renderTarget.guest.baseArrayLayer = static_cast<u16>(baseArrayLayer);
            renderTarget.view.reset();
        }

        TextureView *GetRenderTarget(RenderTarget &renderTarget) {
            if (renderTarget.disabled)
                return nullptr;
            else if (renderTarget.view)
                return &*renderTarget.view;

            if (renderTarget.guest.mappings.empty()) {
                size_t layerStride{renderTarget.guest.GetLayerSize()};
                size_t size{layerStride * (renderTarget.guest.layerCount - renderTarget.guest.baseArrayLayer)};
                auto mappings{channelCtx.asCtx->gmmu.TranslateRange(renderTarget.iova, size)};
                renderTarget.guest.mappings.assign(mappings.begin(), mappings.end());
            }

            renderTarget.guest.type = static_cast<texture::TextureType>(renderTarget.guest.dimensions.GetType());

            renderTarget.view = gpu.texture.FindOrCreate(renderTarget.guest);
            return renderTarget.view.get();
        }

        TextureView *GetColorRenderTarget(size_t index) {
            return GetRenderTarget(colorRenderTargets.at(index));
        }

        TextureView *GetDepthRenderTarget() {
            return GetRenderTarget(depthRenderTarget);
        }

        void UpdateRenderTargetControl(maxwell3d::RenderTargetControl control) {
            renderTargetControl = control;
        }

        /* Viewport */
      private:
        bool viewportOriginLowerLeft{}; //!< If the viewport origin follows the lower left convention (OpenGL) as opposed to upper left (Vulkan/Direct3D)
        std::array<bool, maxwell3d::ViewportCount> viewportsFlipY{}; //!< If the Y axis of a viewport has been flipped via a viewport swizzle
        std::array<vk::Viewport, maxwell3d::ViewportCount> viewports;
        std::array<vk::Rect2D, maxwell3d::ViewportCount> scissors; //!< The scissors applied to viewports/render targets for masking writes during draws or clears
        constexpr static vk::Rect2D DefaultScissor{
            .extent.height = std::numeric_limits<i32>::max(),
            .extent.width = std::numeric_limits<i32>::max(),
        }; //!< A scissor which displays the entire viewport, utilized when the viewport scissor is disabled

      public:
        /**
         * @url https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#vertexpostproc-viewport
         * @note Comments are written in the way of getting the same viewport transformations to be done on the host rather than deriving the host structure values from the guest submitted values, fundamentally the same thing but it is consistent with not assuming a certain guest API
         */
        void SetViewportX(size_t index, float scale, float translate) {
            auto &viewport{viewports.at(index)};
            viewport.x = translate - scale; // Counteract the addition of the half of the width (o_x) to the host translation
            viewport.width = scale * 2.0f; // Counteract the division of the width (p_x) by 2 for the host scale
        }

        void SetViewportY(size_t index, float scale, float translate) {
            auto &viewport{viewports.at(index)};
            viewport.y = translate - scale; // Counteract the addition of the half of the height (p_y/2 is center) to the host translation (o_y)
            viewport.height = scale * 2.0f; // Counteract the division of the height (p_y) by 2 for the host scale
            if (viewportOriginLowerLeft ^ viewportsFlipY[index]) {
                // Flip the viewport given that the viewport origin is lower left or the viewport Y has been flipped via a swizzle but not if both are active at the same time
                viewport.y += viewport.height;
                viewport.height = -viewport.height;
            }
        }

        void SetViewportZ(size_t index, float scale, float translate) {
            auto &viewport{viewports.at(index)};
            viewport.minDepth = translate; // minDepth (o_z) directly corresponds to the host translation
            viewport.maxDepth = scale + translate; // Counteract the subtraction of the maxDepth (p_z - o_z) by minDepth (o_z) for the host scale
        }

        void SetViewportSwizzle(size_t index, maxwell3d::ViewportTransform::Swizzle x, maxwell3d::ViewportTransform::Swizzle y, maxwell3d::ViewportTransform::Swizzle z, maxwell3d::ViewportTransform::Swizzle w) {
            using Swizzle = maxwell3d::ViewportTransform::Swizzle;
            if (x != Swizzle::PositiveX && y != Swizzle::PositiveY && y != Swizzle::NegativeY && z != Swizzle::PositiveZ && w != Swizzle::PositiveW)
                throw exception("Unsupported viewport swizzle: {}x{}x{}", maxwell3d::ViewportTransform::ToString(x), maxwell3d::ViewportTransform::ToString(y), maxwell3d::ViewportTransform::ToString(z));

            bool shouldFlipY{y == Swizzle::NegativeY};

            auto &viewportFlipY{viewportsFlipY[index]};
            if (viewportFlipY != shouldFlipY) {
                auto &viewport{viewports[index]};
                viewport.y += viewport.height;
                viewport.height = -viewport.height;

                viewportFlipY = shouldFlipY;
            }
        }

        void SetViewportOrigin(bool isLowerLeft) {
            if (viewportOriginLowerLeft != isLowerLeft) {
                for (auto &viewport : viewports) {
                    viewport.y += viewport.height;
                    viewport.height = -viewport.height;
                }
                viewportOriginLowerLeft = isLowerLeft;
            }
        }

        void SetScissor(size_t index, std::optional<maxwell3d::Scissor> scissor) {
            scissors.at(index) = scissor ? vk::Rect2D{
                .offset.x = scissor->horizontal.minimum,
                .extent.width = static_cast<u32>(scissor->horizontal.maximum - scissor->horizontal.minimum),
                .offset.y = scissor->vertical.minimum,
                .extent.height = static_cast<u32>(scissor->horizontal.maximum - scissor->vertical.minimum),
            } : DefaultScissor;
        }

        void SetScissorHorizontal(size_t index, maxwell3d::Scissor::ScissorBounds bounds) {
            auto &scissor{scissors.at(index)};
            scissor.offset.x = bounds.minimum;
            scissor.extent.width = bounds.maximum - bounds.minimum;
        }

        void SetScissorVertical(size_t index, maxwell3d::Scissor::ScissorBounds bounds) {
            auto &scissor{scissors.at(index)};
            scissor.offset.y = bounds.minimum;
            scissor.extent.height = bounds.maximum - bounds.minimum;
        }

        /* Buffer Clears */
      private:
        vk::ClearColorValue clearColorValue{}; //!< The value written to the color RT being cleared
        vk::ClearDepthStencilValue clearDepthValue{}; //!< The value written to the depth/stencil RT being cleared

      public:
        void UpdateClearColorValue(size_t index, u32 value) {
            clearColorValue.uint32.at(index) = value;
        }

        void UpdateClearDepthValue(float depth) {
            clearDepthValue.depth = depth;
        }

        void UpdateClearStencilValue(u32 stencil) {
            clearDepthValue.stencil = stencil;
        }

        void ClearColorRt(TextureView *renderTarget, vk::Rect2D scissor, u32 layerIndex) {
            std::lock_guard lock(*renderTarget);
            executor.AttachTexture(renderTarget);

            scissor.extent.width = static_cast<u32>(std::min(static_cast<i32>(renderTarget->texture->dimensions.width) - scissor.offset.x,
                                                             static_cast<i32>(scissor.extent.width)));
            scissor.extent.height = static_cast<u32>(std::min(static_cast<i32>(renderTarget->texture->dimensions.height) - scissor.offset.y,
                                                              static_cast<i32>(scissor.extent.height)));

            if (scissor.extent.width != 0 && scissor.extent.height != 0) {
                if (scissor.extent.width == renderTarget->texture->dimensions.width && scissor.extent.height == renderTarget->texture->dimensions.height && renderTarget->range.baseArrayLayer == 0 && renderTarget->range.layerCount == 1 && layerIndex == 0) {
                    executor.AddClearColorSubpass(renderTarget, clearColorValue);
                } else {
                    vk::ClearAttachment clearAttachment{
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .colorAttachment = 0,
                        .clearValue = clearColorValue,
                    };

                    vk::ClearRect clearRect{
                        .rect = scissor,
                        .baseArrayLayer = layerIndex,
                        .layerCount = 1,
                    };

                    executor.AddSubpass([clearAttachment, clearRect](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32) {
                        commandBuffer.clearAttachments(clearAttachment, clearRect);
                    }, vk::Rect2D{.extent = renderTarget->texture->dimensions}, {}, renderTarget);
                }
            }
        }

        void ClearDepthStencilRt(TextureView *renderTarget, vk::ImageAspectFlags aspect, u32 layerIndex) {
            std::lock_guard lock(*renderTarget);
            executor.AttachTexture(renderTarget);

            if (renderTarget->range.layerCount == 1 && layerIndex == 0) {
                executor.AddClearDepthStencilSubpass(renderTarget, clearDepthValue);
            } else {
                vk::ClearAttachment clearAttachment{
                    .aspectMask = aspect,
                    .clearValue = clearDepthValue,
                };

                auto &dimensions{renderTarget->texture->dimensions};
                vk::Rect2D imageArea{
                    .extent = vk::Extent2D{
                        .width = dimensions.width,
                        .height = dimensions.height
                    },
                };

                vk::ClearRect clearRect{
                    .rect = imageArea,
                    .baseArrayLayer = layerIndex,
                    .layerCount = 1,
                };

                executor.AddSubpass([clearAttachment, clearRect](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32) {
                    commandBuffer.clearAttachments(clearAttachment, clearRect);
                }, imageArea, {}, {}, renderTarget);
            }
        }

        void ClearBuffers(maxwell3d::ClearBuffers clear) {
            bool isColor{clear.red || clear.green || clear.blue || clear.alpha};
            auto renderTargetIndex{renderTargetControl[clear.renderTargetId]};
            auto colorRenderTargetView{isColor ? GetColorRenderTarget(renderTargetIndex) : nullptr};

            if (colorRenderTargetView) {
                if (!clear.red || !clear.green || !clear.blue || !clear.alpha)
                    throw exception("Atomically clearing color channels is not supported ({}{}{}{})", clear.red ? 'R' : '-', clear.green ? 'G' : '-', clear.blue ? 'B' : '-', clear.alpha ? 'A' : '-');

                if (colorRenderTargetView->format->vkAspect & vk::ImageAspectFlagBits::eColor)
                    ClearColorRt(colorRenderTargetView, scissors.at(renderTargetIndex), clear.layerId);
            }

            bool isDepth{clear.depth || clear.stencil};
            auto depthRenderTargetView{isDepth ? GetDepthRenderTarget() : nullptr};

            if (depthRenderTargetView) {
                vk::ImageAspectFlags aspect{};
                if (clear.depth)
                    aspect |= vk::ImageAspectFlagBits::eDepth;
                if (clear.stencil)
                    aspect |= vk::ImageAspectFlagBits::eStencil;

                aspect &= depthRenderTargetView->format->vkAspect;
                if (aspect)
                    ClearDepthStencilRt(depthRenderTargetView, aspect, clear.layerId);
            }
        }

        /* Constant Buffers */
      private:
        struct ConstantBuffer {
            IOVA iova;
            u32 size;
            GuestBuffer guest;
            std::shared_ptr<BufferView> view;

            /**
             * @brief Reads an object from the supplied offset in the constant buffer
             * @note This must only be called when the GuestBuffer is resolved correctly
             */
            template<typename T>
            T Read(size_t offset) {
                T object;
                size_t objectOffset{};
                for (auto &mapping: guest.mappings) {
                    if (offset < mapping.size_bytes()) {
                        auto copySize{std::min(mapping.size_bytes() - offset, sizeof(T))};
                        std::memcpy(reinterpret_cast<u8 *>(&object) + objectOffset, mapping.data() + offset, copySize);
                        objectOffset += copySize;
                        if (objectOffset == sizeof(T))
                            return object;
                        offset = mapping.size_bytes();
                    } else {
                        offset -= mapping.size_bytes();
                    }
                }
                throw exception("Object extent ({} + {} = {}) is larger than constant buffer size: {}", size + offset, sizeof(T), size + offset + sizeof(T), size);
            }

            /**
             * @brief Writes an object to the supplied offset in the constant buffer
             * @note This must only be called when the GuestBuffer is resolved correctly
             */
            template<typename T>
            void Write(const T &object, size_t offset) {
                size_t objectOffset{};
                for (auto &mapping: guest.mappings) {
                    if (offset < mapping.size_bytes()) {
                        auto copySize{std::min(mapping.size_bytes() - offset, sizeof(T))};
                        std::memcpy(mapping.data() + offset, reinterpret_cast<const u8 *>(&object) + objectOffset, copySize);
                        objectOffset += copySize;
                        if (objectOffset == sizeof(T))
                            return;
                        offset = mapping.size_bytes();
                    } else {
                        offset -= mapping.size_bytes();
                    }
                }
                throw exception("Object extent ({} + {} = {}) is larger than constant buffer size: {}", size + offset, sizeof(T), size + offset + sizeof(T), size);
            }
        };
        ConstantBuffer constantBufferSelector; //!< The constant buffer selector is used to bind a constant buffer to a stage or update data in it

        u32 constantBufferUpdateOffset{}; //!< The offset at which any inline constant buffer updata data is written

      public:
        void SetConstantBufferSelectorSize(u32 size) {
            constantBufferSelector.size = size;
            constantBufferSelector.view.reset();
        }

        void SetConstantBufferSelectorIovaHigh(u32 high) {
            constantBufferSelector.iova.high = high;
            constantBufferSelector.view.reset();
        }

        void SetConstantBufferSelectorIovaLow(u32 low) {
            constantBufferSelector.iova.low = low;
            constantBufferSelector.view.reset();
        }

        std::optional<ConstantBuffer> GetConstantBufferSelector() {
            if (constantBufferSelector.size == 0)
                return std::nullopt;
            else if (constantBufferSelector.view)
                return constantBufferSelector;

            auto mappings{channelCtx.asCtx->gmmu.TranslateRange(constantBufferSelector.iova, constantBufferSelector.size)};

            // Ignore unmapped areas from mappings due to buggy games setting the wrong cbuf size
            mappings.erase(ranges::find_if(mappings, [](const auto &mapping) { return !mapping.valid(); }), mappings.end());

            constantBufferSelector.guest.mappings.assign(mappings.begin(), mappings.end());

            constantBufferSelector.view = gpu.buffer.FindOrCreate(constantBufferSelector.guest);
            return constantBufferSelector;
        }

        void SetConstantBufferUpdateOffset(u32 offset) {
            constantBufferUpdateOffset = offset;
        }

        void ConstantBufferUpdate(u32 data) {
            auto constantBuffer{GetConstantBufferSelector().value()};
            constantBuffer.Write(data, constantBufferUpdateOffset);
        }

        /* Shader Program */
      private:
        constexpr static size_t MaxShaderBytecodeSize{1 * 1024 * 1024}; //!< The largest shader binary that we support (1 MiB)

        struct Shader {
            bool enabled{false};
            ShaderCompiler::Stage stage;

            bool invalidated{true}; //!< If the shader that existed earlier has been invalidated
            bool shouldCheckSame{false}; //!< If we should do a check for the shader being the same as before
            u32 offset{}; //!< Offset of the shader from the base IOVA
            boost::container::static_vector<u8, MaxShaderBytecodeSize> data; //!< The shader bytecode in a statically allocated vector
            std::shared_ptr<ShaderManager::ShaderProgram> program{};

            Shader(ShaderCompiler::Stage stage) : stage(stage) {}

            maxwell3d::PipelineStage ToPipelineStage() {
                using ShaderStage = ShaderCompiler::Stage;
                using PipelineStage = maxwell3d::PipelineStage;

                switch (stage) {
                    case ShaderStage::VertexA:
                    case ShaderStage::VertexB:
                        return PipelineStage::Vertex;

                    case ShaderStage::TessellationControl:
                        return PipelineStage::TessellationControl;
                    case ShaderStage::TessellationEval:
                        return PipelineStage::TessellationEvaluation;

                    case ShaderStage::Geometry:
                        return PipelineStage::Geometry;

                    case ShaderStage::Fragment:
                        return PipelineStage::Fragment;

                    case ShaderStage::Compute:
                        throw exception("Unexpected compute shader in Maxwell3D");
                }
            }
        };

        struct ShaderSet : public std::array<Shader, maxwell3d::ShaderStageCount> {
            ShaderSet() : array{
                Shader{ShaderCompiler::Stage::VertexA},
                Shader{ShaderCompiler::Stage::VertexB},
                Shader{ShaderCompiler::Stage::TessellationControl},
                Shader{ShaderCompiler::Stage::TessellationEval},
                Shader{ShaderCompiler::Stage::Geometry},
                Shader{ShaderCompiler::Stage::Fragment},
            } {}

            Shader &at(maxwell3d::ShaderStage stage) {
                return array::at(static_cast<size_t>(stage));
            }

            Shader &operator[](maxwell3d::ShaderStage stage) {
                return array::operator[](static_cast<size_t>(stage));
            }
        };

        struct PipelineStage {
            bool enabled{false};
            vk::ShaderStageFlagBits vkStage;

            std::shared_ptr<ShaderManager::ShaderProgram> program; //!< The shader program by value or by reference (VertexA and VertexB shaders when combined will store by value, otherwise only a reference is stored)

            bool needsRecompile{}; //!< If the shader needs to be recompiled as runtime information has changed
            ShaderCompiler::VaryingState previousStageStores{};
            u32 bindingBase{}, bindingLast{}; //!< The base and last binding for descriptors bound to this stage
            std::shared_ptr<vk::raii::ShaderModule> vkModule;

            std::array<ConstantBuffer, maxwell3d::PipelineStageConstantBufferCount> constantBuffers{};

            PipelineStage(vk::ShaderStageFlagBits vkStage) : vkStage(vkStage) {}
        };

        struct PipelineStages : public std::array<PipelineStage, maxwell3d::PipelineStageCount> {
            PipelineStages() : array{
                PipelineStage{vk::ShaderStageFlagBits::eVertex},
                PipelineStage{vk::ShaderStageFlagBits::eTessellationControl},
                PipelineStage{vk::ShaderStageFlagBits::eTessellationEvaluation},
                PipelineStage{vk::ShaderStageFlagBits::eGeometry},
                PipelineStage{vk::ShaderStageFlagBits::eFragment},
            } {}

            PipelineStage &at(maxwell3d::PipelineStage stage) {
                return array::at(static_cast<size_t>(stage));
            }

            PipelineStage &operator[](maxwell3d::PipelineStage stage) {
                return array::operator[](static_cast<size_t>(stage));
            }
        };

        IOVA shaderBaseIova{}; //!< The base IOVA that shaders are located at an offset from
        ShaderSet shaders;
        PipelineStages pipelineStages;

        ShaderCompiler::RuntimeInfo runtimeInfo{};

        constexpr static size_t PipelineUniqueDescriptorTypeCount{2}; //!< The amount of unique descriptor types that may be bound to a pipeline
        constexpr static size_t MaxPipelineDescriptorWriteCount{maxwell3d::PipelineStageCount * PipelineUniqueDescriptorTypeCount}; //!< The maxium amount of descriptors writes that are used to bind a pipeline
        constexpr static size_t MaxPipelineDescriptorCount{100}; //!< The maxium amount of descriptors we support being bound to a pipeline

        boost::container::static_vector<vk::WriteDescriptorSet, MaxPipelineDescriptorWriteCount> descriptorSetWrites;
        boost::container::static_vector<vk::DescriptorSetLayoutBinding, MaxPipelineDescriptorCount> layoutBindings;
        boost::container::static_vector<vk::DescriptorBufferInfo, MaxPipelineDescriptorCount> bufferInfo;
        boost::container::static_vector<vk::DescriptorImageInfo, MaxPipelineDescriptorCount> imageInfo;

        /**
         * @brief All state concerning the shader programs and their bindings
         * @note The `descriptorSetWrite` will have a null `dstSet` which needs to be assigned prior to usage
         */
        struct ShaderProgramState {
            boost::container::static_vector<std::shared_ptr<vk::raii::ShaderModule>, maxwell3d::PipelineStageCount> shaderModules; //!< Shader modules for every pipeline stage
            boost::container::static_vector<vk::PipelineShaderStageCreateInfo, maxwell3d::PipelineStageCount> shaderStages; //!< Shader modules for every pipeline stage
            vk::raii::DescriptorSetLayout descriptorSetLayout; //!< The descriptor set layout for the pipeline (Only valid when `activeShaderStagesInfoCount` is non-zero)
            span<vk::WriteDescriptorSet> descriptorSetWrites; //!< The writes to the descriptor set that need to be done prior to executing a pipeline
        };

        /**
         * @brief Updates `runtimeInfo` while automatically triggering a recompilation for a stage if the value has been updated
         * @param member A member of `runtimeInfo` passed by reference which will be checked and set
         * @param value The new value of the member
         * @param stages All the shader stages that need to be recompiled if this value is updated
         */
        template<typename T, class... Args>
        void UpdateRuntimeInformation(T &member, T value, Args... stages) {
            if (member != value) {
                member = value;

                auto setStageRecompile{[this](maxwell3d::PipelineStage stage) {
                    pipelineStages[stage].needsRecompile = true;
                }};
                (setStageRecompile(stages), ...);
            }
        }

        /**
         * @note The return value of previous calls will be invalidated on a call to this as values are provided by reference
         * @note Any bound resources will automatically be attached to the CommandExecutor, there's no need to manually attach them
         */
        ShaderProgramState CompileShaderProgramState() {
            for (auto &shader : shaders) {
                auto &pipelineStage{pipelineStages[shader.ToPipelineStage()]};
                if (shader.enabled) {
                    // We only want to include the shader if it is enabled on the guest
                    if (shader.invalidated) {
                        // If a shader is invalidated, we need to reparse the program (given that it has changed)

                        bool shouldParseShader{[&]() {
                            if (!shader.data.empty() && shader.shouldCheckSame) {
                                // A fast path to check if the shader is the same as before to avoid reparsing the shader
                                auto newIovaRanges{channelCtx.asCtx->gmmu.TranslateRange(shaderBaseIova + shader.offset, shader.data.size())};
                                auto originalShader{shader.data.data()};

                                for (auto &range : newIovaRanges) {
                                    if (range.data() && std::memcmp(range.data(), originalShader, range.size()) == 0) {
                                        originalShader += range.size();
                                    } else {
                                        return true;
                                    }
                                }

                                return false;
                            } else {
                                shader.shouldCheckSame = true; // We want to reset the value and check for it being same the next time
                                return true;
                            }
                        }()};

                        if (shouldParseShader) {
                            // A pass to check if the shader has a BRA infloop opcode ending (On most commercial games)
                            shader.data.resize(MaxShaderBytecodeSize);
                            auto foundEnd{channelCtx.asCtx->gmmu.ReadTill(shader.data, shaderBaseIova + shader.offset, [](span<u8> data) -> std::optional<size_t> {
                                // We attempt to find the shader size by looking for "BRA $" (Infinite Loop) which is used as padding at the end of the shader
                                // UAM Shader Compiler Reference: https://github.com/devkitPro/uam/blob/5a5afc2bae8b55409ab36ba45be63fcb73f68993/source/compiler_iface.cpp#L319-L351
                                constexpr u64 BraSelf1{0xE2400FFFFF87000F}, BraSelf2{0xE2400FFFFF07000F};

                                span<u64> shaderInstructions{data.cast<u64, std::dynamic_extent, true>()};
                                for (auto it{shaderInstructions.begin()}; it != shaderInstructions.end(); it++) {
                                    auto instruction{*it};
                                    if (instruction == BraSelf1 || instruction == BraSelf2) [[unlikely]]
                                        // It is far more likely that the instruction doesn't match so this is an unlikely case
                                        return static_cast<size_t>(std::distance(shaderInstructions.begin(), it)) * sizeof(u64);
                                }
                                return std::nullopt;
                            })};

                            shader.program = gpu.shader.ParseGraphicsShader(shader.stage, shader.data, shader.offset, bindlessTextureConstantBufferIndex);

                            if (shader.stage != ShaderCompiler::Stage::VertexA && shader.stage != ShaderCompiler::Stage::VertexB) {
                                pipelineStage.program = shader.program;
                            } else if (shader.stage == ShaderCompiler::Stage::VertexA) {
                                auto &vertexB{shaders[maxwell3d::ShaderStage::VertexB]};

                                if (!vertexB.enabled)
                                    throw exception("Enabling VertexA without VertexB is not supported");
                                else if (!vertexB.invalidated)
                                    // If only VertexA is invalidated, we need to recombine here but we can defer it otherwise
                                    pipelineStage.program = gpu.shader.CombineVertexShaders(shader.program, vertexB.program, vertexB.data);
                            } else if (shader.stage == ShaderCompiler::Stage::VertexB) {
                                auto &vertexA{shaders[maxwell3d::ShaderStage::VertexA]};

                                if (vertexA.enabled)
                                    // We need to combine the vertex shader stages if VertexA is enabled
                                    pipelineStage.program = gpu.shader.CombineVertexShaders(vertexA.program, shader.program, shader.data);
                                else
                                    pipelineStage.program = shader.program;
                            }

                            pipelineStage.enabled = true;
                            pipelineStage.needsRecompile = true;
                        }

                        shader.invalidated = false;
                    }
                } else if (shader.stage != ShaderCompiler::Stage::VertexA) {
                    pipelineStage.enabled = false;
                }
            }

            descriptorSetWrites.clear();
            layoutBindings.clear();
            bufferInfo.clear();
            imageInfo.clear();

            runtimeInfo.previous_stage_stores.mask.set(); // First stage should always have all bits set
            ShaderCompiler::Backend::Bindings bindings{};

            boost::container::static_vector<std::shared_ptr<vk::raii::ShaderModule>, maxwell3d::PipelineStageCount> shaderModules;
            boost::container::static_vector<vk::PipelineShaderStageCreateInfo, maxwell3d::PipelineStageCount> shaderStages;
            for (auto &pipelineStage : pipelineStages) {
                if (!pipelineStage.enabled)
                    continue;

                if (pipelineStage.needsRecompile || bindings.unified != pipelineStage.bindingBase || pipelineStage.previousStageStores.mask != runtimeInfo.previous_stage_stores.mask) {
                    pipelineStage.previousStageStores = runtimeInfo.previous_stage_stores;
                    pipelineStage.bindingBase = bindings.unified;
                    pipelineStage.vkModule = std::make_shared<vk::raii::ShaderModule>(gpu.shader.CompileShader(runtimeInfo, pipelineStage.program, bindings));
                    pipelineStage.bindingLast = bindings.unified;
                }

                auto &program{pipelineStage.program->program};
                runtimeInfo.previous_stage_stores = program.info.stores;
                if (program.is_geometry_passthrough)
                    runtimeInfo.previous_stage_stores.mask |= program.info.passthrough.mask;
                bindings.unified = pipelineStage.bindingLast;

                u32 bindingIndex{pipelineStage.bindingBase};
                if (!program.info.constant_buffer_descriptors.empty()) {
                    descriptorSetWrites.push_back(vk::WriteDescriptorSet{
                        .dstBinding = bindingIndex,
                        .descriptorCount = static_cast<u32>(program.info.constant_buffer_descriptors.size()),
                        .descriptorType = vk::DescriptorType::eUniformBuffer,
                        .pBufferInfo = bufferInfo.data() + bufferInfo.size(),
                    });

                    for (auto &constantBuffer : program.info.constant_buffer_descriptors) {
                        layoutBindings.push_back(vk::DescriptorSetLayoutBinding{
                            .binding = bindingIndex++,
                            .descriptorType = vk::DescriptorType::eUniformBuffer,
                            .descriptorCount = 1,
                            .stageFlags = pipelineStage.vkStage,
                        });

                        auto view{pipelineStage.constantBuffers[constantBuffer.index].view};
                        std::scoped_lock lock(*view);
                        bufferInfo.push_back(vk::DescriptorBufferInfo{
                            .buffer = view->buffer->GetBacking(),
                            .offset = view->offset,
                            .range = view->range,
                        });
                        executor.AttachBuffer(view.get());
                    }
                }

                if (!program.info.texture_descriptors.empty()) {
                    descriptorSetWrites.push_back(vk::WriteDescriptorSet{
                        .dstBinding = bindingIndex,
                        .descriptorCount = static_cast<u32>(program.info.texture_descriptors.size()),
                        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                        .pImageInfo = imageInfo.data() + imageInfo.size(),
                    });

                    u32 descriptorIndex{};
                    for (auto &texture : program.info.texture_descriptors) {
                        layoutBindings.push_back(vk::DescriptorSetLayoutBinding{
                            .binding = bindingIndex++,
                            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                            .descriptorCount = 1,
                            .stageFlags = pipelineStage.vkStage,
                        });

                        auto &constantBuffer{pipelineStage.constantBuffers[texture.cbuf_index]};
                        union TextureHandle {
                            u32 raw;
                            struct {
                                u32 textureIndex : 20;
                                u32 samplerIndex : 12;
                            };
                        } handle{constantBuffer.Read<u32>(texture.cbuf_offset + (descriptorIndex++ << texture.size_shift))};

                        auto sampler{GetSampler(handle.samplerIndex)};
                        auto textureView{GetPoolTextureView(handle.textureIndex)};

                        std::scoped_lock lock(*textureView);
                        imageInfo.push_back(vk::DescriptorImageInfo{
                            .sampler = **sampler,
                            .imageView = textureView->GetView(),
                            .imageLayout = textureView->texture->layout,
                        });
                        executor.AttachTexture(textureView.get());
                        executor.AttachDependency(std::move(sampler));
                    }
                }

                shaderModules.emplace_back(pipelineStage.vkModule);
                shaderStages.emplace_back(vk::PipelineShaderStageCreateInfo{
                    .stage = pipelineStage.vkStage,
                    .module = **pipelineStage.vkModule,
                    .pName = "main",
                });
            }

            return {
                std::move(shaderModules),
                std::move(shaderStages),
                vk::raii::DescriptorSetLayout(gpu.vkDevice, vk::DescriptorSetLayoutCreateInfo{
                    .pBindings = layoutBindings.data(),
                    .bindingCount = static_cast<u32>(layoutBindings.size()),
                }),
                descriptorSetWrites,
            };
        }

      public:
        void SetShaderBaseIovaHigh(u32 high) {
            shaderBaseIova.high = high;
            for (auto &shader : shaders) {
                shader.invalidated = true;
                shader.shouldCheckSame = false;
            }
        }

        void SetShaderBaseIovaLow(u32 low) {
            shaderBaseIova.low = low;
            for (auto &shader : shaders) {
                shader.invalidated = true;
                shader.shouldCheckSame = false;
            }
        }

        void SetShaderEnabled(maxwell3d::ShaderStage stage, bool enabled) {
            auto &shader{shaders[stage]};
            shader.enabled = enabled;
            shader.invalidated = true;
        }

        void SetShaderOffset(maxwell3d::ShaderStage stage, u32 offset) {
            auto &shader{shaders[stage]};
            shader.offset = offset;
            shader.invalidated = true;
        }

        void BindPipelineConstantBuffer(maxwell3d::PipelineStage stage, bool enable, u32 index) {
            auto &constantBuffer{pipelineStages[stage].constantBuffers[index]};

            if (enable) {
                constantBuffer = GetConstantBufferSelector().value();
            } else {
                constantBuffer = {};
            }
        }

        /* Rasterizer State */
      private:
        vk::StructureChain<vk::PipelineRasterizationStateCreateInfo, vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT> rasterizerState;
        bool cullFaceEnabled{};
        vk::CullModeFlags cullMode{}; //!< The current cull mode regardless of it being enabled or disabled
        vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT provokingVertexState{};
        bool depthBiasPoint{}, depthBiasLine{}, depthBiasFill{};

      public:
        void SetDepthClampEnabled(bool enabled) {
            rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>().depthClampEnable = enabled;
        }

        vk::PolygonMode ConvertPolygonMode(maxwell3d::PolygonMode mode) {
            switch (mode) {
                case maxwell3d::PolygonMode::Point:
                    return vk::PolygonMode::ePoint;
                case maxwell3d::PolygonMode::Line:
                    return vk::PolygonMode::eLine;
                case maxwell3d::PolygonMode::Fill:
                    return vk::PolygonMode::eFill;
            }
        }

        void SetPolygonModeFront(maxwell3d::PolygonMode mode) {
            rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>().polygonMode = ConvertPolygonMode(mode);
        }

        void SetPolygonModeBack(maxwell3d::PolygonMode mode) {
            auto frontPolygonMode{rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>().polygonMode};
            auto backPolygonMode{ConvertPolygonMode(mode)};
            if (frontPolygonMode != backPolygonMode)
                Logger::Warn("Cannot set back-facing polygon mode ({}) different from front-facing polygon mode ({}) due to Vulkan constraints", vk::to_string(backPolygonMode), vk::to_string(frontPolygonMode));
        }

        void SetCullFaceEnabled(bool enabled) {
            cullFaceEnabled = enabled;
            if (!enabled)
                rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>().cullMode = {};
        }

        void SetFrontFace(maxwell3d::FrontFace face) {
            rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>().frontFace = [face]() {
                switch (face) {
                    case maxwell3d::FrontFace::Clockwise:
                        return vk::FrontFace::eClockwise;
                    case maxwell3d::FrontFace::CounterClockwise:
                        return vk::FrontFace::eCounterClockwise;
                }
            }();
        }

        void SetCullFace(maxwell3d::CullFace face) {
            cullMode = [face]() -> vk::CullModeFlags {
                switch (face) {
                    case maxwell3d::CullFace::Front:
                        return vk::CullModeFlagBits::eFront;
                    case maxwell3d::CullFace::Back:
                        return vk::CullModeFlagBits::eBack;
                    case maxwell3d::CullFace::FrontAndBack:
                        return vk::CullModeFlagBits::eFrontAndBack;
                }
            }();
            if (cullFaceEnabled)
                rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>().cullMode = cullMode;
        }

        void SetProvokingVertex(bool isLast) {
            if (isLast) {
                if (!gpu.quirks.supportsLastProvokingVertex)
                    Logger::Warn("Cannot set provoking vertex to last without host GPU support");
                rasterizerState.get<vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT>().provokingVertexMode = vk::ProvokingVertexModeEXT::eLastVertex;
            } else {
                rasterizerState.get<vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT>().provokingVertexMode = vk::ProvokingVertexModeEXT::eFirstVertex;
            }
        }

        void SetLineWidth(float width) {
            rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>().lineWidth = width;
        }

        void SetDepthBiasPointEnabled(bool enabled) {
            depthBiasPoint = enabled;
        }

        void SetDepthBiasLineEnabled(bool enabled) {
            depthBiasLine = enabled;
        }

        void SetDepthBiasFillEnabled(bool enabled) {
            depthBiasFill = enabled;
        }

        void SetDepthBiasConstantFactor(float factor) {
            rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>().depthBiasConstantFactor = factor;
        }

        void SetDepthBiasClamp(float clamp) {
            rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>().depthBiasClamp = clamp;
        }

        void SetDepthBiasSlopeFactor(float factor) {
            rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>().depthBiasSlopeFactor = factor;
        }

        /* Color Blending */
      private:
        std::array<vk::PipelineColorBlendAttachmentState, maxwell3d::RenderTargetCount> commonRtBlendState{}, independentRtBlendState{}; //!< Per-RT blending state for common/independent blending for trivial toggling behavior
        vk::PipelineColorBlendStateCreateInfo blendState{
            .pAttachments = commonRtBlendState.data(),
            .attachmentCount = maxwell3d::RenderTargetCount,
        };

      public:
        void SetBlendLogicOpEnable(bool enabled) {
            if (!gpu.quirks.supportsLogicOp && enabled) {
                Logger::Warn("Cannot enable framebuffer logical operation without host GPU support");
                return;
            }
            blendState.logicOpEnable = enabled;
        }

        void SetBlendLogicOpType(maxwell3d::ColorLogicOp logicOp) {
            blendState.logicOp = [logicOp]() {
                switch (logicOp) {
                    case maxwell3d::ColorLogicOp::Clear:
                        return vk::LogicOp::eClear;
                    case maxwell3d::ColorLogicOp::And:
                        return vk::LogicOp::eAnd;
                    case maxwell3d::ColorLogicOp::AndReverse:
                        return vk::LogicOp::eAndReverse;
                    case maxwell3d::ColorLogicOp::Copy:
                        return vk::LogicOp::eCopy;
                    case maxwell3d::ColorLogicOp::AndInverted:
                        return vk::LogicOp::eAndInverted;
                    case maxwell3d::ColorLogicOp::Noop:
                        return vk::LogicOp::eNoOp;
                    case maxwell3d::ColorLogicOp::Xor:
                        return vk::LogicOp::eXor;
                    case maxwell3d::ColorLogicOp::Or:
                        return vk::LogicOp::eOr;
                    case maxwell3d::ColorLogicOp::Nor:
                        return vk::LogicOp::eNor;
                    case maxwell3d::ColorLogicOp::Equiv:
                        return vk::LogicOp::eEquivalent;
                    case maxwell3d::ColorLogicOp::Invert:
                        return vk::LogicOp::eInvert;
                    case maxwell3d::ColorLogicOp::OrReverse:
                        return vk::LogicOp::eOrReverse;
                    case maxwell3d::ColorLogicOp::CopyInverted:
                        return vk::LogicOp::eCopyInverted;
                    case maxwell3d::ColorLogicOp::OrInverted:
                        return vk::LogicOp::eOrInverted;
                    case maxwell3d::ColorLogicOp::Nand:
                        return vk::LogicOp::eNand;
                    case maxwell3d::ColorLogicOp::Set:
                        return vk::LogicOp::eSet;
                }
            }();
        }

        void SetAlphaTestEnabled(bool enable) {
            if (enable)
                Logger::Warn("Cannot enable alpha testing due to Vulkan constraints");
        }

        vk::BlendOp ConvertBlendOp(maxwell3d::BlendOp op) {
            switch (op) {
                case maxwell3d::BlendOp::Add:
                case maxwell3d::BlendOp::AddGL:
                    return vk::BlendOp::eAdd;

                case maxwell3d::BlendOp::Subtract:
                case maxwell3d::BlendOp::SubtractGL:
                    return vk::BlendOp::eSubtract;

                case maxwell3d::BlendOp::ReverseSubtract:
                case maxwell3d::BlendOp::ReverseSubtractGL:
                    return vk::BlendOp::eReverseSubtract;

                case maxwell3d::BlendOp::Minimum:
                case maxwell3d::BlendOp::MinimumGL:
                    return vk::BlendOp::eMin;

                case maxwell3d::BlendOp::Maximum:
                case maxwell3d::BlendOp::MaximumGL:
                    return vk::BlendOp::eMax;
            }
        }

        vk::BlendFactor ConvertBlendFactor(maxwell3d::BlendFactor factor) {
            switch (factor) {
                case maxwell3d::BlendFactor::Zero:
                case maxwell3d::BlendFactor::ZeroGL:
                    return vk::BlendFactor::eZero;

                case maxwell3d::BlendFactor::One:
                case maxwell3d::BlendFactor::OneGL:
                    return vk::BlendFactor::eOne;

                case maxwell3d::BlendFactor::SourceColor:
                case maxwell3d::BlendFactor::SourceColorGL:
                    return vk::BlendFactor::eSrcColor;

                case maxwell3d::BlendFactor::OneMinusSourceColor:
                case maxwell3d::BlendFactor::OneMinusSourceColorGL:
                    return vk::BlendFactor::eOneMinusSrcColor;

                case maxwell3d::BlendFactor::SourceAlpha:
                case maxwell3d::BlendFactor::SourceAlphaGL:
                    return vk::BlendFactor::eSrcAlpha;

                case maxwell3d::BlendFactor::OneMinusSourceAlpha:
                case maxwell3d::BlendFactor::OneMinusSourceAlphaGL:
                    return vk::BlendFactor::eOneMinusSrcAlpha;

                case maxwell3d::BlendFactor::DestAlpha:
                case maxwell3d::BlendFactor::DestAlphaGL:
                    return vk::BlendFactor::eDstAlpha;

                case maxwell3d::BlendFactor::OneMinusDestAlpha:
                case maxwell3d::BlendFactor::OneMinusDestAlphaGL:
                    return vk::BlendFactor::eOneMinusDstAlpha;

                case maxwell3d::BlendFactor::DestColor:
                case maxwell3d::BlendFactor::DestColorGL:
                    return vk::BlendFactor::eDstColor;

                case maxwell3d::BlendFactor::OneMinusDestColor:
                case maxwell3d::BlendFactor::OneMinusDestColorGL:
                    return vk::BlendFactor::eOneMinusDstColor;

                case maxwell3d::BlendFactor::SourceAlphaSaturate:
                case maxwell3d::BlendFactor::SourceAlphaSaturateGL:
                    return vk::BlendFactor::eSrcAlphaSaturate;

                case maxwell3d::BlendFactor::Source1Color:
                case maxwell3d::BlendFactor::Source1ColorGL:
                    return vk::BlendFactor::eSrc1Color;

                case maxwell3d::BlendFactor::OneMinusSource1Color:
                case maxwell3d::BlendFactor::OneMinusSource1ColorGL:
                    return vk::BlendFactor::eOneMinusSrc1Color;

                case maxwell3d::BlendFactor::Source1Alpha:
                case maxwell3d::BlendFactor::Source1AlphaGL:
                    return vk::BlendFactor::eSrc1Alpha;

                case maxwell3d::BlendFactor::OneMinusSource1Alpha:
                case maxwell3d::BlendFactor::OneMinusSource1AlphaGL:
                    return vk::BlendFactor::eOneMinusSrc1Alpha;

                case maxwell3d::BlendFactor::ConstantColor:
                case maxwell3d::BlendFactor::ConstantColorGL:
                    return vk::BlendFactor::eConstantColor;

                case maxwell3d::BlendFactor::OneMinusConstantColor:
                case maxwell3d::BlendFactor::OneMinusConstantColorGL:
                    return vk::BlendFactor::eOneMinusConstantColor;

                case maxwell3d::BlendFactor::ConstantAlpha:
                case maxwell3d::BlendFactor::ConstantAlphaGL:
                    return vk::BlendFactor::eConstantAlpha;

                case maxwell3d::BlendFactor::OneMinusConstantAlpha:
                case maxwell3d::BlendFactor::OneMinusConstantAlphaGL:
                    return vk::BlendFactor::eOneMinusConstantAlpha;
            }
        }

        void SetIndependentBlendingEnabled(bool enable) {
            if (enable)
                blendState.pAttachments = independentRtBlendState.data();
            else
                blendState.pAttachments = commonRtBlendState.data();
        }

        void SetColorBlendEnabled(bool enable) {
            for (auto &blend : commonRtBlendState)
                blend.blendEnable = enable;
        }

        void SetColorBlendOp(maxwell3d::BlendOp op) {
            auto vkOp{ConvertBlendOp(op)};
            for (auto &blend : commonRtBlendState)
                blend.colorBlendOp = vkOp;
        }

        void SetSrcColorBlendFactor(maxwell3d::BlendFactor factor) {
            auto vkFactor{ConvertBlendFactor(factor)};
            for (auto &blend : commonRtBlendState)
                blend.srcColorBlendFactor = vkFactor;
        }

        void SetDstColorBlendFactor(maxwell3d::BlendFactor factor) {
            auto vkFactor{ConvertBlendFactor(factor)};
            for (auto &blend : commonRtBlendState)
                blend.dstColorBlendFactor = vkFactor;
        }

        void SetAlphaBlendOp(maxwell3d::BlendOp op) {
            auto vkOp{ConvertBlendOp(op)};
            for (auto &blend : commonRtBlendState)
                blend.alphaBlendOp = vkOp;
        }

        void SetSrcAlphaBlendFactor(maxwell3d::BlendFactor factor) {
            auto vkFactor{ConvertBlendFactor(factor)};
            for (auto &blend : commonRtBlendState)
                blend.srcAlphaBlendFactor = vkFactor;
        }

        void SetDstAlphaBlendFactor(maxwell3d::BlendFactor factor) {
            auto vkFactor{ConvertBlendFactor(factor)};
            for (auto &blend : commonRtBlendState)
                blend.dstAlphaBlendFactor = vkFactor;
        }

        void SetColorBlendEnabled(u32 index, bool enable) {
            independentRtBlendState[index].blendEnable = enable;
        }

        void SetColorBlendOp(u32 index, maxwell3d::BlendOp op) {
            independentRtBlendState[index].colorBlendOp = ConvertBlendOp(op);
        }

        void SetSrcColorBlendFactor(u32 index, maxwell3d::BlendFactor factor) {
            independentRtBlendState[index].srcColorBlendFactor = ConvertBlendFactor(factor);
        }

        void SetDstColorBlendFactor(u32 index, maxwell3d::BlendFactor factor) {
            independentRtBlendState[index].dstColorBlendFactor = ConvertBlendFactor(factor);
        }

        void SetAlphaBlendOp(u32 index, maxwell3d::BlendOp op) {
            independentRtBlendState[index].alphaBlendOp = ConvertBlendOp(op);
        }

        void SetSrcAlphaBlendFactor(u32 index, maxwell3d::BlendFactor factor) {
            independentRtBlendState[index].srcAlphaBlendFactor = ConvertBlendFactor(factor);
        }

        void SetDstAlphaBlendFactor(u32 index, maxwell3d::BlendFactor factor) {
            independentRtBlendState[index].dstAlphaBlendFactor = ConvertBlendFactor(factor);
        }

        void SetColorWriteMask(u32 index, maxwell3d::ColorWriteMask mask) {
            vk::ColorComponentFlags colorWriteMask{};
            if (mask.red)
                colorWriteMask |= vk::ColorComponentFlagBits::eR;
            if (mask.green)
                colorWriteMask |= vk::ColorComponentFlagBits::eG;
            if (mask.blue)
                colorWriteMask |= vk::ColorComponentFlagBits::eB;
            if (mask.alpha)
                colorWriteMask |= vk::ColorComponentFlagBits::eA;

            // While blending state might include the color write mask on Vulkan, they are separate on Maxwell and this results in even `commonRtBlendState` requiring the `independentBlend` feature in certain circumstances where blending state might be the same but with independent color write masks
            independentRtBlendState[index].colorWriteMask = colorWriteMask;
            commonRtBlendState[index].colorWriteMask = colorWriteMask;
        }

        void SetColorBlendConstant(u32 index, float constant) {
            blendState.blendConstants[index] = constant;
        }

        /* Vertex Buffers */
      private:
        struct VertexBuffer {
            vk::VertexInputBindingDescription bindingDescription{};
            vk::VertexInputBindingDivisorDescriptionEXT bindingDivisorDescription{};
            IOVA start{}, end{}; //!< IOVAs covering a contiguous region in GPU AS with the vertex buffer
            std::shared_ptr<BufferView> view;
        };
        std::array<VertexBuffer, maxwell3d::VertexBufferCount> vertexBuffers{};

        struct VertexAttribute {
            bool enabled{};
            vk::VertexInputAttributeDescription description;
        };
        std::array<VertexAttribute, maxwell3d::VertexAttributeCount> vertexAttributes{};

      public:
        void SetVertexBufferStride(u32 index, u32 stride) {
            vertexBuffers[index].bindingDescription.stride = stride;
        }

        void SetVertexBufferInputRate(u32 index, bool isPerInstance) {
            vertexBuffers[index].bindingDescription.inputRate = isPerInstance ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex;
        }

        void SetVertexBufferStartIovaHigh(u32 index, u32 high) {
            auto &vertexBuffer{vertexBuffers[index]};
            vertexBuffer.start.high = high;
            vertexBuffer.view.reset();
        }

        void SetVertexBufferStartIovaLow(u32 index, u32 low) {
            auto &vertexBuffer{vertexBuffers[index]};
            vertexBuffer.start.low = low;
            vertexBuffer.view.reset();
        }

        void SetVertexBufferEndIovaHigh(u32 index, u32 high) {
            auto &vertexBuffer{vertexBuffers[index]};
            vertexBuffer.end.high = high;
            vertexBuffer.view.reset();
        }

        void SetVertexBufferEndIovaLow(u32 index, u32 low) {
            auto &vertexBuffer{vertexBuffers[index]};
            vertexBuffer.end.low = low;
            vertexBuffer.view.reset();
        }

        void SetVertexBufferDivisor(u32 index, u32 divisor) {
            if (!gpu.quirks.supportsVertexAttributeDivisor)
                Logger::Warn("Cannot set vertex attribute divisor without host GPU support");
            else if (divisor == 0 && !gpu.quirks.supportsVertexAttributeZeroDivisor)
                Logger::Warn("Cannot set vertex attribute divisor to zero without host GPU support");
            vertexBuffers[index].bindingDivisorDescription.divisor = divisor;
        }

        vk::Format ConvertVertexBufferFormat(maxwell3d::VertexAttribute::ElementType type, maxwell3d::VertexAttribute::ElementSize size) {
            using Size = maxwell3d::VertexAttribute::ElementSize;
            using Type = maxwell3d::VertexAttribute::ElementType;

            if (size == Size::e0 || type == Type::None)
                return vk::Format::eUndefined;

            #define FORMAT_CASE(size, type, vkType, vkFormat, ...) \
                case Size::size | Type::type: \
                    return vk::Format::vkFormat ## vkType ##__VA_ARGS__

            #define FORMAT_INT_CASE(size, vkFormat, ...) \
                FORMAT_CASE(size, Uint, Uint, vkFormat, ##__VA_ARGS__); \
                FORMAT_CASE(size, Sint, Sint, vkFormat, ##__VA_ARGS__);

            #define FORMAT_INT_FLOAT_CASE(size, vkFormat, ...) \
                FORMAT_INT_CASE(size, vkFormat, ##__VA_ARGS__); \
                FORMAT_CASE(size, Float, Sfloat, vkFormat, ##__VA_ARGS__);

            #define FORMAT_NORM_INT_SCALED_CASE(size, vkFormat, ...) \
                FORMAT_INT_CASE(size, vkFormat, ##__VA_ARGS__);               \
                FORMAT_CASE(size, Unorm, Unorm, vkFormat, ##__VA_ARGS__);     \
                FORMAT_CASE(size, Snorm, Unorm, vkFormat, ##__VA_ARGS__);     \
                FORMAT_CASE(size, Uscaled, Uscaled, vkFormat, ##__VA_ARGS__); \
                FORMAT_CASE(size, Sscaled, Sscaled, vkFormat, ##__VA_ARGS__)

            #define FORMAT_NORM_INT_SCALED_FLOAT_CASE(size, vkFormat) \
                FORMAT_NORM_INT_SCALED_CASE(size, vkFormat); \
                FORMAT_CASE(size, Float, Sfloat, vkFormat)

            switch (size | type) {
                // @fmt:off

                /* 8-bit components */
                FORMAT_NORM_INT_SCALED_CASE(e1x8, eR8);
                FORMAT_NORM_INT_SCALED_CASE(e2x8, eR8G8);
                FORMAT_NORM_INT_SCALED_CASE(e3x8, eR8G8B8);
                FORMAT_NORM_INT_SCALED_CASE(e4x8, eR8G8B8A8);

                /* 16-bit components */
                FORMAT_NORM_INT_SCALED_FLOAT_CASE(e1x16, eR16);
                FORMAT_NORM_INT_SCALED_FLOAT_CASE(e2x16, eR16G16);
                FORMAT_NORM_INT_SCALED_FLOAT_CASE(e3x16, eR16G16B16);
                FORMAT_NORM_INT_SCALED_FLOAT_CASE(e4x16, eR16G16B16A16);

                /* 32-bit components */
                FORMAT_INT_FLOAT_CASE(e1x32, eR32);
                FORMAT_INT_FLOAT_CASE(e2x32, eR32G32);
                FORMAT_INT_FLOAT_CASE(e3x32, eR32G32B32);
                FORMAT_INT_FLOAT_CASE(e4x32, eR32G32B32A32);

                /* 10-bit RGB, 2-bit A */
                FORMAT_NORM_INT_SCALED_CASE(e10_10_10_2, eA2B10G10R10, Pack32);

                /* Unknown */
                case 0x12F: return vk::Format::eUndefined; // Issued by Maxwell3D::InitializeRegisters()

                // @fmt:on

                default:
                    throw exception("Unimplemented Maxwell3D Vertex Buffer Format: {} | {}", maxwell3d::VertexAttribute::ToString(size), maxwell3d::VertexAttribute::ToString(type));
            }

            #undef FORMAT_CASE
            #undef FORMAT_INT_CASE
            #undef FORMAT_INT_FLOAT_CASE
            #undef FORMAT_NORM_INT_SCALED_CASE
            #undef FORMAT_NORM_INT_SCALED_FLOAT_CASE
        }

        void SetVertexAttributeState(u32 index, maxwell3d::VertexAttribute attribute) {
            auto &vertexAttribute{vertexAttributes[index]};
            if (!attribute.isConstant) {
                vertexAttribute.enabled = true;
                vertexAttribute.description.binding = attribute.bufferId;
                vertexAttribute.description.format = ConvertVertexBufferFormat(attribute.type, attribute.elementSize);
                vertexAttribute.description.offset = attribute.offset;

                auto inputType{[type = attribute.type]() {
                    using MaxwellType = maxwell3d::VertexAttribute::ElementType;
                    using ShaderType = ShaderCompiler::AttributeType;

                    switch (type) {
                        case MaxwellType::None:
                            return ShaderType::Disabled;
                        case MaxwellType::Snorm:
                        case MaxwellType::Unorm:
                        case MaxwellType::Uscaled:
                        case MaxwellType::Sscaled:
                        case MaxwellType::Float:
                            return ShaderType::Float;
                        case MaxwellType::Sint:
                            return ShaderType::SignedInt;
                        case MaxwellType::Uint:
                            return ShaderType::UnsignedInt;
                    }
                }()};

                UpdateRuntimeInformation(runtimeInfo.generic_input_types[index], inputType, maxwell3d::PipelineStage::Vertex);
            } else {
                vertexAttribute.enabled = false;
            }
        }

        BufferView *GetVertexBuffer(size_t index) {
            auto &vertexBuffer{vertexBuffers.at(index)};
            if (vertexBuffer.start > vertexBuffer.end || vertexBuffer.start == 0 || vertexBuffer.end == 0)
                return nullptr;
            else if (vertexBuffer.view)
                return vertexBuffer.view.get();

            GuestBuffer guest;
            auto mappings{channelCtx.asCtx->gmmu.TranslateRange(vertexBuffer.start, (vertexBuffer.end + 1) - vertexBuffer.start)};
            guest.mappings.assign(mappings.begin(), mappings.end());

            vertexBuffer.view = gpu.buffer.FindOrCreate(guest);
            return vertexBuffer.view.get();
        }

        /* Input Assembly */
      private:
        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState{};

      public:
        void SetPrimitiveTopology(maxwell3d::PrimitiveTopology topology) {
            auto[vkTopology, shaderTopology] = [topology]() -> std::tuple<vk::PrimitiveTopology, ShaderCompiler::InputTopology> {
                using MaxwellTopology = maxwell3d::PrimitiveTopology;
                using VkTopology = vk::PrimitiveTopology;
                using ShaderTopology = ShaderCompiler::InputTopology;
                switch (topology) {
                    // @fmt:off

                    case MaxwellTopology::PointList: return {VkTopology::ePointList, ShaderTopology::Points};

                    case MaxwellTopology::LineList: return {VkTopology::eLineList, ShaderTopology::Lines};
                    case MaxwellTopology::LineStrip: return {VkTopology::eLineStrip, ShaderTopology::Lines};
                    case MaxwellTopology::LineListWithAdjacency: return {VkTopology::eLineListWithAdjacency, ShaderTopology::LinesAdjacency};
                    case MaxwellTopology::LineStripWithAdjacency: return {VkTopology::eLineStripWithAdjacency, ShaderTopology::LinesAdjacency};

                    case MaxwellTopology::TriangleList: return {VkTopology::eTriangleList, ShaderTopology::Triangles};
                    case MaxwellTopology::TriangleStrip: return {VkTopology::eTriangleStrip, ShaderTopology::Triangles};
                    case MaxwellTopology::TriangleFan: return {VkTopology::eTriangleFan, ShaderTopology::Triangles};
                    case MaxwellTopology::TriangleListWithAdjacency: return {VkTopology::eTriangleListWithAdjacency, ShaderTopology::TrianglesAdjacency};
                    case MaxwellTopology::TriangleStripWithAdjacency: return {VkTopology::eTriangleStripWithAdjacency, ShaderTopology::TrianglesAdjacency};

                    case MaxwellTopology::PatchList: return {VkTopology::ePatchList, ShaderTopology::Triangles};

                    // @fmt:on

                    default:
                        throw exception("Unimplemented Maxwell3D Primitive Topology: {}", maxwell3d::ToString(topology));
                }
            }();

            inputAssemblyState.topology = vkTopology;
            UpdateRuntimeInformation(runtimeInfo.input_topology, shaderTopology, maxwell3d::PipelineStage::Geometry);
        }

        /* Index Buffer */
      private:
        struct IndexBuffer {
            IOVA start{}, end{}; //!< IOVAs covering a contiguous region in GPU AS containing the index buffer (end does not represent the true extent of the index buffers, just a maximum possible extent and is set to extremely high values which cannot be used to create a buffer)
            vk::IndexType type{};
            vk::DeviceSize viewSize{}; //!< The size of the cached view
            std::shared_ptr<BufferView> view{}; //!< A cached view tied to the IOVAs and size to allow for a faster lookup

            vk::DeviceSize GetIndexBufferSize(u32 elementCount) {
                switch (type) {
                    case vk::IndexType::eUint8EXT:
                        return sizeof(u8) * elementCount;
                    case vk::IndexType::eUint16:
                        return sizeof(u16) * elementCount;
                    case vk::IndexType::eUint32:
                        return sizeof(u32) * elementCount;

                    default:
                        throw exception("Unsupported Vulkan Index Type: {}", vk::to_string(type));
                }
            }
        } indexBuffer{};

        /* Textures */
      private:
        u32 bindlessTextureConstantBufferIndex{};

        struct PoolTexture : public FenceCycleDependency {
            GuestTexture guest;
            std::weak_ptr<TextureView> view;
        };

        struct TexturePool {
            IOVA iova;
            u32 maximumIndex;
            span<TextureImageControl> imageControls;
            std::unordered_map<TextureImageControl, PoolTexture, util::ObjectHash<TextureImageControl>> textures;
        } texturePool{};

      public:
        void SetBindlessTextureConstantBufferIndex(u32 index) {
            bindlessTextureConstantBufferIndex = index;
        }

        void SetTexturePoolIovaHigh(u32 high) {
            texturePool.iova.high = high;
            texturePool.imageControls = nullptr;
        }

        void SetTexturePoolIovaLow(u32 low) {
            texturePool.iova.low = low;
            texturePool.imageControls = nullptr;
        }

        void SetTexturePoolMaximumIndex(u32 index) {
            texturePool.maximumIndex = index;
            texturePool.imageControls = nullptr;
        }

      private:
        texture::Format ConvertTicFormat(TextureImageControl::FormatWord format) {
            using TIC = TextureImageControl;
            #define TIC_FORMAT(format, componentR, componentG, componentB, componentA, swizzleX, swizzleY, swizzleZ, swizzleW) \
                TIC::FormatWord{TIC::ImageFormat::format,                                                                                                           \
                                TIC::ImageComponent::componentR, TIC::ImageComponent::componentG, TIC::ImageComponent::componentB, TIC::ImageComponent::componentA, \
                                TIC::ImageSwizzle::swizzleX, TIC::ImageSwizzle::swizzleY, TIC::ImageSwizzle::swizzleZ, TIC::ImageSwizzle::swizzleW}.Raw()

            // For formats where all components are of the same type
            #define TIC_FORMAT_ST(format, component, swizzleX, swizzleY, swizzleZ, swizzleW) \
                TIC_FORMAT(format, component, component, component, component, swizzleX, swizzleY, swizzleZ, swizzleW)

            #define TIC_FORMAT_CASE(ticFormat, skFormat, componentR, componentG, componentB, componentA, swizzleX, swizzleY, swizzleZ, swizzleW)  \
                case TIC_FORMAT(ticFormat, component, component, component, component, swizzleX, swizzleY, swizzleZ, swizzleW): \
                    return format::skFormat

            #define TIC_FORMAT_CASE_ST(ticFormat, skFormat, component, swizzleX, swizzleY, swizzleZ, swizzleW)  \
                case TIC_FORMAT_ST(ticFormat, component, swizzleX, swizzleY, swizzleZ, swizzleW): \
                    return format::skFormat ## component

            #define TIC_FORMAT_CASE_NORM(ticFormat, skFormat, swizzleX, swizzleY, swizzleZ, swizzleW)  \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Unorm, swizzleX, swizzleY, swizzleZ, swizzleW); \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Snorm, swizzleX, swizzleY, swizzleZ, swizzleW)

            #define TIC_FORMAT_CASE_INT(ticFormat, skFormat, swizzleX, swizzleY, swizzleZ, swizzleW)  \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Uint, swizzleX, swizzleY, swizzleZ, swizzleW); \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Sint, swizzleX, swizzleY, swizzleZ, swizzleW)

            #define TIC_FORMAT_CASE_NORM_INT(ticFormat, skFormat, swizzleX, swizzleY, swizzleZ, swizzleW) \
                TIC_FORMAT_CASE_NORM(ticFormat, skFormat, swizzleX, swizzleY, swizzleZ, swizzleW); \
                TIC_FORMAT_CASE_INT(ticFormat, skFormat, swizzleX, swizzleY, swizzleZ, swizzleW)

            #define TIC_FORMAT_CASE_NORM_INT_FLOAT(ticFormat, skFormat, swizzleX, swizzleY, swizzleZ, swizzleW) \
                TIC_FORMAT_CASE_NORM_INT(ticFormat, skFormat, swizzleX, swizzleY, swizzleZ, swizzleW); \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Float, swizzleX, swizzleY, swizzleZ, swizzleW)

            switch (format.Raw()) {
                TIC_FORMAT_CASE_NORM_INT(R8, R8R001, R, Zero, Zero, OneFloat);
                TIC_FORMAT_CASE_ST(B5G6R5, R5G6B5, Unorm, B, G, R, OneFloat);
                TIC_FORMAT_CASE_NORM_INT(A8R8G8B8, R8G8B8A8, R, G, B, A);
                TIC_FORMAT_CASE_NORM_INT(A8R8G8B8, A8B8G8R8, A, R, G, B);
                TIC_FORMAT_CASE_NORM_INT(A8R8G8B8, G8B8A8R8, G, B, A, R);
                TIC_FORMAT_CASE_NORM_INT_FLOAT(R16G16B16A16, R16G16B16A16, R, G, B, A);
                TIC_FORMAT_CASE_NORM_INT(A2B10G10R10, A2B10G10R10, R, G, B, A);
                TIC_FORMAT_CASE_ST(Astc4x4, Astc4x4, Unorm, R, G, B, A);
                TIC_FORMAT_CASE_ST(Dxt1, Bc1, Unorm, R, G, B, A);
                TIC_FORMAT_CASE_ST(Dxt23, Bc2, Unorm, R, G, B, A);
                TIC_FORMAT_CASE_ST(Dxt45, Bc3, Unorm, R, G, B, A);
                TIC_FORMAT_CASE_ST(Dxn1, Bc4111R, Unorm, OneFloat, OneFloat, OneFloat, R);
                TIC_FORMAT_CASE_ST(Dxn1, Bc4RRR1, Unorm, R, R, R, OneFloat);
                TIC_FORMAT_CASE_ST(BC7U, Bc7, Unorm, R, G, B, A);
                TIC_FORMAT_CASE_ST(ZF32, D32, Float, R, R, R, OneFloat);

                default:
                    throw exception("Cannot translate TIC format: 0x{:X}", static_cast<u32>(format.Raw()));
            }

            #undef TIC_FORMAT
            #undef TIC_FORMAT_ST
            #undef TIC_FORMAT_CASE
            #undef TIC_FORMAT_CASE_ST
            #undef TIC_FORMAT_CASE_NORM
            #undef TIC_FORMAT_CASE_INT
            #undef TIC_FORMAT_CASE_NORM_INT
            #undef TIC_FORMAT_CASE_NORM_INT_FLOAT
        }

        std::shared_ptr<TextureView> GetPoolTextureView(u32 index) {
            if (!texturePool.imageControls.valid()) {
                auto mappings{channelCtx.asCtx->gmmu.TranslateRange(texturePool.iova, texturePool.maximumIndex * sizeof(TextureImageControl))};
                if (mappings.size() != 1)
                    throw exception("Texture pool mapping count is unexpected: {}", mappings.size());
                texturePool.imageControls = mappings.front().cast<TextureImageControl>();
            }

            TextureImageControl &textureControl{texturePool.imageControls[index]};
            auto textureIt{texturePool.textures.insert({textureControl, {}})};
            auto &poolTexture{textureIt.first->second};
            if (textureIt.second) {
                // If the entry didn't exist prior then we need to convert the TIC to a GuestTexture
                auto &guest{poolTexture.guest};
                guest.format = ConvertTicFormat(textureControl.formatWord);

                constexpr size_t CubeFaceCount{6}; //!< The amount of faces of a cube

                guest.baseArrayLayer = static_cast<u16>(textureControl.BaseLayer());
                guest.dimensions = texture::Dimensions(textureControl.widthMinusOne + 1, textureControl.heightMinusOne + 1, 1);
                u16 depth{static_cast<u16>(textureControl.depthMinusOne + 1)};

                using TicType = TextureImageControl::TextureType;
                using TexType = texture::TextureType;
                switch (textureControl.textureType) {
                    case TicType::e1D:
                        guest.type = TexType::e1D;
                        guest.layerCount = 1;
                        break;
                    case TicType::e1DArray:
                        guest.type = TexType::e1D;
                        guest.layerCount = depth;
                        break;
                    case TicType::e1DBuffer:
                        throw exception("1D Buffers are not supported");

                    case TicType::e2D:
                    case TicType::e2DNoMipmap:
                        guest.type = TexType::e2D;
                        guest.layerCount = 1;
                        break;
                    case TicType::e2DArray:
                        guest.type = TexType::e2D;
                        guest.layerCount = depth;
                        break;

                    case TicType::e3D:
                        guest.type = TexType::e3D;
                        guest.layerCount = 1;
                        guest.dimensions.depth = depth;
                        break;

                    case TicType::eCubemap:
                        guest.type = TexType::e2D;
                        guest.layerCount = CubeFaceCount;
                        break;
                    case TicType::eCubeArray:
                        guest.type = TexType::e2D;
                        guest.layerCount = depth * CubeFaceCount;
                        break;
                }

                size_t size; //!< The size of the texture in bytes
                if (textureControl.headerType == TextureImageControl::HeaderType::Pitch) {
                    guest.tileConfig = {
                        .mode = texture::TileMode::Pitch,
                        .pitch = static_cast<u32>(textureControl.tileConfig.pitchHigh) << TextureImageControl::TileConfig::PitchAlignmentBits,
                    };
                } else if (textureControl.headerType == TextureImageControl::HeaderType::BlockLinear) {
                    guest.tileConfig = {
                        .mode = texture::TileMode::Block,
                        .blockHeight = static_cast<u8>(1U << textureControl.tileConfig.tileHeightGobsLog2),
                        .blockDepth = static_cast<u8>(1U << textureControl.tileConfig.tileDepthGobsLog2),
                    };
                } else {
                    throw exception("Unsupported TIC Header Type: {}", static_cast<u32>(textureControl.headerType));
                }

                auto mappings{channelCtx.asCtx->gmmu.TranslateRange(textureControl.Iova(), guest.GetLayerSize() * (guest.layerCount - guest.baseArrayLayer))};
                guest.mappings.assign(mappings.begin(), mappings.end());
            } else if (auto textureView{poolTexture.view.lock()}; textureView != nullptr) {
                // If the entry already exists and the view is still valid then we return it directly
                return textureView;
            }

            auto textureView{gpu.texture.FindOrCreate(poolTexture.guest)};
            poolTexture.view = textureView;
            return textureView;
        }

        /* Samplers */
      private:
        struct Sampler : public vk::raii::Sampler, public FenceCycleDependency {
            using vk::raii::Sampler::Sampler;
        };

        struct SamplerPool {
            IOVA iova;
            u32 maximumIndex;
            span<TextureSamplerControl> samplerControls;
            std::unordered_map<TextureSamplerControl, std::shared_ptr<Sampler>, util::ObjectHash<TextureSamplerControl>> samplers;
        } samplerPool{};

      public:
        void SetSamplerPoolIovaHigh(u32 high) {
            samplerPool.iova.high = high;
            samplerPool.samplerControls = nullptr;
        }

        void SetSamplerPoolIovaLow(u32 low) {
            samplerPool.iova.low = low;
            samplerPool.samplerControls = nullptr;
        }

        void SetSamplerPoolMaximumIndex(u32 index) {
            samplerPool.maximumIndex = index;
            samplerPool.samplerControls = nullptr;
        }

      private:
        vk::Filter ConvertSamplerFilter(TextureSamplerControl::Filter filter) {
            using TscFilter = TextureSamplerControl::Filter;
            using VkFilter = vk::Filter;
            switch (filter) {
                // @fmt:off

                case TscFilter::Nearest: return VkFilter::eNearest;
                case TscFilter::Linear: return VkFilter::eLinear;

                // @fmt:on
            }
        }

        vk::SamplerMipmapMode ConvertSamplerMipFilter(TextureSamplerControl::MipFilter filter) {
            using TscFilter = TextureSamplerControl::MipFilter;
            using VkMode = vk::SamplerMipmapMode;
            switch (filter) {
                // @fmt:off

                case TscFilter::None: return VkMode{};
                case TscFilter::Nearest: return VkMode::eNearest;
                case TscFilter::Linear: return VkMode::eLinear;

                // @fmt:on
            }
        }

        vk::SamplerAddressMode ConvertSamplerAddressMode(TextureSamplerControl::AddressMode mode) {
            using TscMode = TextureSamplerControl::AddressMode;
            using VkMode = vk::SamplerAddressMode;
            switch (mode) {
                // @fmt:off

                case TscMode::Repeat: return VkMode::eRepeat;
                case TscMode::MirroredRepeat: return VkMode::eMirroredRepeat;

                case TscMode::ClampToEdge: return VkMode::eClampToEdge;
                case TscMode::ClampToBorder: return VkMode::eClampToBorder;
                case TscMode::Clamp: return VkMode::eClampToEdge; // Vulkan doesn't support 'GL_CLAMP' so this is an approximation

                case TscMode::MirrorClampToEdge: return VkMode::eMirrorClampToEdge;
                case TscMode::MirrorClampToBorder: return VkMode::eMirrorClampToEdge; // Only supported mirror clamps are to edges so this is an approximation
                case TscMode::MirrorClamp: return VkMode::eMirrorClampToEdge; // Same as above

                // @fmt:on
            }
        }

        vk::CompareOp ConvertSamplerCompareOp(TextureSamplerControl::CompareOp compareOp) {
            using TscOp = TextureSamplerControl::CompareOp;
            using VkOp = vk::CompareOp;
            switch (compareOp) {
                // @fmt:off

                case TscOp::Never: return VkOp::eNever;
                case TscOp::Less: return VkOp::eLess;
                case TscOp::Equal: return VkOp::eEqual;
                case TscOp::LessOrEqual: return VkOp::eLessOrEqual;
                case TscOp::Greater: return VkOp::eGreater;
                case TscOp::NotEqual: return VkOp::eNotEqual;
                case TscOp::GreaterOrEqual: return VkOp::eGreaterOrEqual;
                case TscOp::Always: return VkOp::eAlways;

                // @fmt:on
            }
        }

        vk::SamplerReductionMode ConvertSamplerReductionFilter(TextureSamplerControl::SamplerReduction reduction) {
            using TscReduction = TextureSamplerControl::SamplerReduction;
            using VkReduction = vk::SamplerReductionMode;
            switch (reduction) {
                // @fmt:off

                case TscReduction::WeightedAverage: return VkReduction::eWeightedAverage;
                case TscReduction::Min: return VkReduction::eMin;
                case TscReduction::Max: return VkReduction::eMax;

                // @fmt:on
            }
        }

        vk::BorderColor ConvertBorderColorWithCustom(float red, float green, float blue, float alpha) {
            if (alpha == 1.0f) {
                if (red == 1.0f && green == 1.0f && blue == 1.0f)
                    return vk::BorderColor::eFloatOpaqueWhite;
                else if (red == 0.0f && green == 0.0f && blue == 0.0f)
                    return vk::BorderColor::eFloatOpaqueBlack;
            } else if (red == 1.0f && green == 1.0f && blue == 1.0f && alpha == 0.0f) {
                return vk::BorderColor::eFloatTransparentBlack;
            }

            return vk::BorderColor::eFloatCustomEXT;
        }

        vk::BorderColor ConvertBorderColorFixed(float red, float green, float blue, float alpha) {
            if (alpha == 1.0f) {
                if (red == 1.0f && green == 1.0f && blue == 1.0f)
                    return vk::BorderColor::eFloatOpaqueWhite;
                else if (red == 0.0f && green == 0.0f && blue == 0.0f)
                    return vk::BorderColor::eFloatOpaqueBlack;
            } else if (red == 1.0f && green == 1.0f && blue == 1.0f && alpha == 0.0f) {
                return vk::BorderColor::eFloatTransparentBlack;
            }

            // Approximations of a custom color using fixed colors
            if (red + green + blue > 1.0f)
                return vk::BorderColor::eFloatOpaqueWhite;
            else if (alpha > 0.0f)
                return vk::BorderColor::eFloatOpaqueBlack;
            else
                return vk::BorderColor::eFloatTransparentBlack;
        }

        std::shared_ptr<Sampler> GetSampler(u32 index) {
            if (!samplerPool.samplerControls.valid()) {
                auto mappings{channelCtx.asCtx->gmmu.TranslateRange(samplerPool.iova, samplerPool.maximumIndex * sizeof(TextureSamplerControl))};
                if (mappings.size() != 1)
                    throw exception("Sampler pool mapping count is unexpected: {}", mappings.size());
                samplerPool.samplerControls = mappings.front().cast<TextureSamplerControl>();
            }

            TextureSamplerControl &samplerControl{samplerPool.samplerControls[index]};
            auto &sampler{samplerPool.samplers[samplerControl]};
            if (sampler)
                return sampler;

            auto convertAddressModeWithCheck{[&](TextureSamplerControl::AddressMode mode) {
                auto vkMode{ConvertSamplerAddressMode(mode)};
                if (vkMode == vk::SamplerAddressMode::eMirrorClampToEdge && !gpu.quirks.supportsSamplerMirrorClampToEdge) [[unlikely]] {
                    Logger::Warn("Cannot use Mirror Clamp To Edge as Sampler Address Mode without host GPU support");
                    return vk::SamplerAddressMode::eClampToEdge; // We use a normal clamp to edge to approximate it
                }
                return vkMode;
            }};

            auto maxAnisotropy{samplerControl.MaxAnisotropy()};
            vk::StructureChain<vk::SamplerCreateInfo, vk::SamplerReductionModeCreateInfoEXT, vk::SamplerCustomBorderColorCreateInfoEXT> samplerInfo{
                vk::SamplerCreateInfo{
                    .magFilter = ConvertSamplerFilter(samplerControl.magFilter),
                    .minFilter = ConvertSamplerFilter(samplerControl.minFilter),
                    .mipmapMode = ConvertSamplerMipFilter(samplerControl.mipFilter),
                    .addressModeU = convertAddressModeWithCheck(samplerControl.addressModeU),
                    .addressModeV = convertAddressModeWithCheck(samplerControl.addressModeV),
                    .addressModeW = convertAddressModeWithCheck(samplerControl.addressModeP),
                    .mipLodBias = samplerControl.MipLodBias(),
                    .anisotropyEnable = maxAnisotropy > 1.0f,
                    .maxAnisotropy = maxAnisotropy,
                    .compareEnable = samplerControl.depthCompareEnable,
                    .compareOp = ConvertSamplerCompareOp(samplerControl.depthCompareOp),
                    .minLod = samplerControl.MinLodClamp(),
                    .maxLod = samplerControl.MaxLodClamp(),
                    .unnormalizedCoordinates = false,
                }, vk::SamplerReductionModeCreateInfoEXT{
                    .reductionMode = ConvertSamplerReductionFilter(samplerControl.reductionFilter),
                }, vk::SamplerCustomBorderColorCreateInfoEXT{
                    .customBorderColor.float32 = {{samplerControl.borderColorR, samplerControl.borderColorG, samplerControl.borderColorB, samplerControl.borderColorA}},
                    .format = vk::Format::eUndefined,
                },
            };

            if (!gpu.quirks.supportsSamplerReductionMode)
                samplerInfo.unlink<vk::SamplerReductionModeCreateInfoEXT>();

            vk::BorderColor &borderColor{samplerInfo.get<vk::SamplerCreateInfo>().borderColor};
            if (gpu.quirks.supportsCustomBorderColor) {
                borderColor = ConvertBorderColorWithCustom(samplerControl.borderColorR, samplerControl.borderColorG, samplerControl.borderColorB, samplerControl.borderColorA);
                if (borderColor != vk::BorderColor::eFloatCustomEXT)
                    samplerInfo.unlink<vk::SamplerCustomBorderColorCreateInfoEXT>();
            } else {
                borderColor = ConvertBorderColorFixed(samplerControl.borderColorR, samplerControl.borderColorG, samplerControl.borderColorB, samplerControl.borderColorA);
                samplerInfo.unlink<vk::SamplerCustomBorderColorCreateInfoEXT>();
            }

            return sampler = std::make_shared<Sampler>(gpu.vkDevice, samplerInfo.get<vk::SamplerCreateInfo>());
        }

      public:
        void SetIndexBufferStartIovaHigh(u32 high) {
            indexBuffer.start.high = high;
            indexBuffer.view.reset();
        }

        void SetIndexBufferStartIovaLow(u32 low) {
            indexBuffer.start.low = low;
            indexBuffer.view.reset();
        }

        void SetIndexBufferEndIovaHigh(u32 high) {
            indexBuffer.end.high = high;
            indexBuffer.view.reset();
        }

        void SetIndexBufferEndIovaLow(u32 low) {
            indexBuffer.end.low = low;
            indexBuffer.view.reset();
        }

        void SetIndexBufferFormat(maxwell3d::IndexBuffer::Format format) {
            indexBuffer.type = [format]() {
                using MaxwellFormat = maxwell3d::IndexBuffer::Format;
                using VkFormat = vk::IndexType;
                switch (format) {
                    case MaxwellFormat::Uint8:
                        return VkFormat::eUint8EXT;
                    case MaxwellFormat::Uint16:
                        return VkFormat::eUint16;
                    case MaxwellFormat::Uint32:
                        return VkFormat::eUint32;
                }
            }();

            if (indexBuffer.type == vk::IndexType::eUint8EXT && !gpu.quirks.supportsUint8Indices)
                throw exception("Cannot use U8 index buffer without host GPU support");

            indexBuffer.view.reset();
        }

        BufferView *GetIndexBuffer(u32 elementCount) {
            auto size{indexBuffer.GetIndexBufferSize(elementCount)};
            if (indexBuffer.start > indexBuffer.end || indexBuffer.start == 0 || indexBuffer.end == 0 || size == 0)
                return nullptr;
            else if (indexBuffer.view && size == indexBuffer.viewSize)
                return indexBuffer.view.get();

            GuestBuffer guestBuffer;
            auto mappings{channelCtx.asCtx->gmmu.TranslateRange(indexBuffer.start, size)};
            guestBuffer.mappings.assign(mappings.begin(), mappings.end());

            indexBuffer.view = gpu.buffer.FindOrCreate(guestBuffer);
            return indexBuffer.view.get();
        }

        /* Depth */
        vk::PipelineDepthStencilStateCreateInfo depthState{};

        void SetDepthTestEnabled(bool enabled) {
            depthState.depthTestEnable = enabled;
        }

        vk::CompareOp ConvertCompareOp(maxwell3d::CompareOp op) {
            using MaxwellOp = maxwell3d::CompareOp;
            using VkOp = vk::CompareOp;
            switch (op) {
                case MaxwellOp::Never:
                case MaxwellOp::NeverGL:
                    return VkOp::eNever;

                case MaxwellOp::Less:
                case MaxwellOp::LessGL:
                    return VkOp::eLess;

                case MaxwellOp::Equal:
                case MaxwellOp::EqualGL:
                    return VkOp::eEqual;

                case MaxwellOp::LessOrEqual:
                case MaxwellOp::LessOrEqualGL:
                    return VkOp::eLessOrEqual;

                case MaxwellOp::Greater:
                case MaxwellOp::GreaterGL:
                    return VkOp::eGreater;

                case MaxwellOp::NotEqual:
                case MaxwellOp::NotEqualGL:
                    return VkOp::eNotEqual;

                case MaxwellOp::GreaterOrEqual:
                case MaxwellOp::GreaterOrEqualGL:
                    return VkOp::eGreaterOrEqual;

                case MaxwellOp::Always:
                case MaxwellOp::AlwaysGL:
                    return VkOp::eAlways;
            }
        }

        void SetDepthTestFunction(maxwell3d::CompareOp function) {
            depthState.depthCompareOp = ConvertCompareOp(function);
        }

        void SetDepthWriteEnabled(bool enabled) {
            depthState.depthWriteEnable = enabled;
        }

        void SetDepthBoundsTestEnabled(bool enabled) {
            depthState.depthBoundsTestEnable = enabled;
        }

        void SetMinDepthBounds(float min) {
            depthState.minDepthBounds = min;
        }

        void SetMaxDepthBounds(float max) {
            depthState.maxDepthBounds = max;
        }

        void SetStencilTestEnabled(bool enabled) {
            depthState.stencilTestEnable = enabled;
        }

        vk::StencilOp ConvertStencilOp(maxwell3d::StencilOp op) {
            using MaxwellOp = maxwell3d::StencilOp;
            using VkOp = vk::StencilOp;
            switch (op) {
                case MaxwellOp::Keep:
                    return VkOp::eKeep;
                case MaxwellOp::Zero:
                    return VkOp::eZero;
                case MaxwellOp::Replace:
                    return VkOp::eReplace;
                case MaxwellOp::IncrementAndClamp:
                    return VkOp::eIncrementAndClamp;
                case MaxwellOp::DecrementAndClamp:
                    return VkOp::eDecrementAndClamp;
                case MaxwellOp::Invert:
                    return VkOp::eInvert;
                case MaxwellOp::IncrementAndWrap:
                    return VkOp::eIncrementAndWrap;
                case MaxwellOp::DecrementAndWrap:
                    return VkOp::eDecrementAndWrap;
            }
        }

        void SetStencilFrontFailOp(maxwell3d::StencilOp op) {
            depthState.front.failOp = ConvertStencilOp(op);
        }

        void SetStencilBackFailOp(maxwell3d::StencilOp op) {
            depthState.back.failOp = ConvertStencilOp(op);
        }

        void SetStencilFrontPassOp(maxwell3d::StencilOp op) {
            depthState.front.passOp = ConvertStencilOp(op);
        }

        void SetStencilBackPassOp(maxwell3d::StencilOp op) {
            depthState.back.passOp = ConvertStencilOp(op);
        }

        void SetStencilFrontDepthFailOp(maxwell3d::StencilOp op) {
            depthState.front.depthFailOp = ConvertStencilOp(op);
        }

        void SetStencilBackDepthFailOp(maxwell3d::StencilOp op) {
            depthState.back.depthFailOp = ConvertStencilOp(op);
        }

        void SetStencilFrontCompareOp(maxwell3d::CompareOp op) {
            depthState.front.compareOp = ConvertCompareOp(op);
        }

        void SetStencilBackCompareOp(maxwell3d::CompareOp op) {
            depthState.back.compareOp = ConvertCompareOp(op);
        }

        void SetStencilFrontCompareMask(u32 mask) {
            depthState.front.compareMask = mask;
        }

        void SetStencilBackCompareMask(u32 mask) {
            depthState.back.compareMask = mask;
        }

        void SetStencilFrontWriteMask(u32 mask) {
            depthState.front.writeMask = mask;
        }

        void SetStencilBackWriteMask(u32 mask) {
            depthState.back.writeMask = mask;
        }

        void SetStencilFrontReference(u32 reference) {
            depthState.front.reference = reference;
        }

        void SetStencilBackReference(u32 reference) {
            depthState.back.reference = reference;
        }

        /* Multisampling */
        vk::PipelineMultisampleStateCreateInfo multisampleState{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
        };

        /* Draws */
      private:
        vk::raii::PipelineCache pipelineCache;

      public:
        template<bool IsIndexed>
        void Draw(u32 count, u32 first, i32 vertexOffset = 0) {
            // Shader + Binding Setup
            auto programState{CompileShaderProgramState()};

            auto descriptorSet{gpu.descriptor.AllocateSet(*programState.descriptorSetLayout)};
            for (auto &descriptorSetWrite : programState.descriptorSetWrites)
                descriptorSetWrite.dstSet = descriptorSet;
            gpu.vkDevice.updateDescriptorSets(programState.descriptorSetWrites, nullptr);

            vk::raii::PipelineLayout pipelineLayout(gpu.vkDevice, vk::PipelineLayoutCreateInfo{
                .pSetLayouts = &*programState.descriptorSetLayout,
                .setLayoutCount = 1,
            });

            vk::Buffer indexBufferHandle;
            vk::DeviceSize indexBufferOffset;
            vk::IndexType indexBufferType;
            if constexpr (IsIndexed) {
                auto indexBufferView{GetIndexBuffer(count)};
                std::scoped_lock lock(*indexBufferView);
                executor.AttachBuffer(indexBufferView);

                indexBufferHandle = indexBufferView->buffer->GetBacking();
                indexBufferOffset = indexBufferView->offset;
                indexBufferType = indexBuffer.type;
            }

            // Vertex Buffer Setup
            std::array<vk::Buffer, maxwell3d::VertexBufferCount> vertexBufferHandles{};
            std::array<vk::DeviceSize, maxwell3d::VertexBufferCount> vertexBufferOffsets{};

            boost::container::static_vector<vk::VertexInputBindingDescription, maxwell3d::VertexBufferCount> vertexBindingDescriptions{};
            boost::container::static_vector<vk::VertexInputBindingDivisorDescriptionEXT, maxwell3d::VertexBufferCount> vertexBindingDivisorsDescriptions{};

            for (u32 index{}; index < maxwell3d::VertexBufferCount; index++) {
                auto vertexBufferView{GetVertexBuffer(index)};
                if (vertexBufferView) {
                    auto &vertexBuffer{vertexBuffers[index]};
                    vertexBindingDescriptions.push_back(vertexBuffer.bindingDescription);
                    vertexBindingDivisorsDescriptions.push_back(vertexBuffer.bindingDivisorDescription);

                    std::scoped_lock vertexBufferLock(*vertexBufferView);
                    vertexBufferHandles[index] = vertexBufferView->buffer->GetBacking();
                    vertexBufferOffsets[index] = vertexBufferView->offset;
                    executor.AttachBuffer(vertexBufferView);
                }
            }

            // Vertex Attribute Setup
            boost::container::static_vector<vk::VertexInputAttributeDescription, maxwell3d::VertexAttributeCount> vertexAttributesDescriptions{};
            for (auto &vertexAttribute : vertexAttributes)
                if (vertexAttribute.enabled)
                    vertexAttributesDescriptions.push_back(vertexAttribute.description);

            // Color Render Target + Blending Setup
            boost::container::static_vector<TextureView *, maxwell3d::RenderTargetCount> activeColorRenderTargets;
            for (u32 index{}; index < maxwell3d::RenderTargetCount; index++) {
                auto renderTarget{GetColorRenderTarget(index)};
                if (renderTarget) {
                    std::scoped_lock lock(*renderTarget);
                    activeColorRenderTargets.push_back(renderTarget);
                    executor.AttachTexture(renderTarget);
                }
            }

            boost::container::static_vector<vk::PipelineColorBlendAttachmentState, maxwell3d::RenderTargetCount> blendAttachmentStates(blendState.pAttachments, blendState.pAttachments + activeColorRenderTargets.size());

            // Depth/Stencil Render Target Setup
            auto depthRenderTargetView{GetDepthRenderTarget()};
            std::optional<std::scoped_lock<TextureView>> depthTargetLock;
            if (depthRenderTargetView)
                depthTargetLock.emplace(*depthRenderTargetView);

            // Draw Persistent Storage
            struct Storage : FenceCycleDependency {
                vk::raii::PipelineLayout pipelineLayout;
                std::optional<vk::raii::Pipeline> pipeline;
                DescriptorAllocator::ActiveDescriptorSet descriptorSet;

                Storage(vk::raii::PipelineLayout &&pipelineLayout, DescriptorAllocator::ActiveDescriptorSet &&descriptorSet) : pipelineLayout(std::move(pipelineLayout)), descriptorSet(std::move(descriptorSet)) {}
            };

            auto storage{std::make_shared<Storage>(std::move(pipelineLayout), std::move(descriptorSet))};

            // Submit Draw
            executor.AddSubpass([=, &vkDevice = gpu.vkDevice, shaderModules = programState.shaderModules, shaderStages = programState.shaderStages, inputAssemblyState = inputAssemblyState, multiViewport = gpu.quirks.supportsMultipleViewports, viewports = viewports, scissors = scissors, rasterizerState = rasterizerState, multisampleState = multisampleState, depthState = depthState, blendState = blendState, storage = std::move(storage), supportsVertexAttributeDivisor = gpu.quirks.supportsVertexAttributeDivisor, vertexBufferHandles = std::move(vertexBufferHandles), vertexBufferOffsets = std::move(vertexBufferOffsets), pipelineCache = *pipelineCache](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle, GPU &, vk::RenderPass renderPass, u32 subpassIndex) mutable {
                vk::StructureChain<vk::PipelineVertexInputStateCreateInfo, vk::PipelineVertexInputDivisorStateCreateInfoEXT> vertexState{
                    vk::PipelineVertexInputStateCreateInfo{
                        .pVertexBindingDescriptions = vertexBindingDescriptions.data(),
                        .vertexBindingDescriptionCount = static_cast<u32>(vertexBindingDescriptions.size()),
                        .pVertexAttributeDescriptions = vertexAttributesDescriptions.data(),
                        .vertexAttributeDescriptionCount = static_cast<u32>(vertexAttributesDescriptions.size()),
                    }, vk::PipelineVertexInputDivisorStateCreateInfoEXT{
                        .pVertexBindingDivisors = vertexBindingDivisorsDescriptions.data(),
                        .vertexBindingDivisorCount = static_cast<u32>(vertexBindingDivisorsDescriptions.size()),
                    }
                };

                if (!supportsVertexAttributeDivisor)
                    vertexState.unlink<vk::PipelineVertexInputDivisorStateCreateInfoEXT>();

                vk::PipelineViewportStateCreateInfo viewportState{
                    .pViewports = viewports.data(),
                    .viewportCount = static_cast<u32>(multiViewport ? maxwell3d::ViewportCount : 1),
                    .pScissors = scissors.data(),
                    .scissorCount = static_cast<u32>(multiViewport ? maxwell3d::ViewportCount : 1),
                };

                blendState.pAttachments = blendAttachmentStates.data();
                blendState.attachmentCount = static_cast<u32>(blendAttachmentStates.size());

                vk::GraphicsPipelineCreateInfo pipelineCreateInfo{
                    .pStages = shaderStages.data(),
                    .stageCount = static_cast<u32>(shaderStages.size()),
                    .pVertexInputState = &vertexState.get<vk::PipelineVertexInputStateCreateInfo>(),
                    .pInputAssemblyState = &inputAssemblyState,
                    .pViewportState = &viewportState,
                    .pRasterizationState = &rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>(),
                    .pMultisampleState = &multisampleState,
                    .pDepthStencilState = &depthState,
                    .pColorBlendState = &blendState,
                    .pDynamicState = nullptr,
                    .layout = *storage->pipelineLayout,
                    .renderPass = renderPass,
                    .subpass = subpassIndex,
                };

                auto pipeline{(*vkDevice).createGraphicsPipeline(pipelineCache, pipelineCreateInfo, nullptr, *vkDevice.getDispatcher())};
                if (pipeline.result != vk::Result::eSuccess)
                    vk::throwResultException(pipeline.result, __builtin_FUNCTION());

                commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.value);

                for (u32 bindingIndex{}; bindingIndex != vertexBufferHandles.size(); bindingIndex++) {
                    // We need to bind all non-null vertex buffers while skipping any null ones
                    if (vertexBufferHandles[bindingIndex]) {
                        u32 bindingEndIndex{bindingIndex + 1};
                        while (bindingEndIndex < vertexBufferHandles.size() && vertexBufferHandles[bindingEndIndex])
                            bindingEndIndex++;

                        u32 bindingCount{bindingEndIndex - bindingIndex};
                        commandBuffer.bindVertexBuffers(bindingIndex, span(vertexBufferHandles.data() + bindingIndex, bindingCount), span(vertexBufferOffsets.data() + bindingIndex, bindingCount));
                    }
                }

                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *storage->pipelineLayout, 0, storage->descriptorSet, nullptr);

                if constexpr (IsIndexed) {
                    commandBuffer.bindIndexBuffer(indexBufferHandle, indexBufferOffset, indexBufferType);
                    commandBuffer.drawIndexed(count, 1, first, vertexOffset, 0);
                } else {
                    commandBuffer.draw(count, 1, first, 0);
                }

                storage->pipeline = vk::raii::Pipeline(vkDevice, pipeline.value);

                cycle->AttachObject(storage);
            }, vk::Rect2D{
                .extent = activeColorRenderTargets[0]->texture->dimensions,
            }, {}, activeColorRenderTargets, depthRenderTargetView);
        }

        void DrawVertex(u32 vertexCount, u32 firstVertex) {
            Draw<false>(vertexCount, firstVertex);
        }

        void DrawIndexed(u32 indexCount, u32 firstIndex, i32 vertexOffset) {
            Draw<true>(indexCount, firstIndex, vertexOffset);
        }
    };
}
