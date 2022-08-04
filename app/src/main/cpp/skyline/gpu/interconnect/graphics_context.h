// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 yuzu Emulator Project (https://github.com/yuzu-emu/)

#pragma once

#include <boost/functional/hash.hpp>
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
#include "conversion/quads.h"

namespace skyline::gpu::interconnect {
    using IOVA = soc::gm20b::IOVA;
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

      public:
        GraphicsContext(GPU &gpu, soc::gm20b::ChannelContext &channelCtx, gpu::interconnect::CommandExecutor &executor) : gpu(gpu), channelCtx(channelCtx), executor(executor) {
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

            if (!gpu.traits.supportsLastProvokingVertex)
                rasterizerState.unlink<vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT>();

            // Set of default parameters for null image which we use instead of a null descriptor since not all devices support that extension
            constexpr texture::Format NullImageFormat{format::R8G8B8A8Unorm};
            constexpr texture::Dimensions NullImageDimensions{1, 1, 1};
            constexpr vk::ImageLayout NullImageInitialLayout{vk::ImageLayout::eUndefined};
            constexpr vk::ImageTiling NullImageTiling{vk::ImageTiling::eOptimal};
            constexpr vk::ImageCreateFlags NullImageFlags{};
            constexpr vk::ImageUsageFlags NullImageUsage{vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled};

            auto vkImage{gpu.memory.AllocateImage(
                {
                    .flags = NullImageFlags,
                    .imageType = vk::ImageType::e2D,
                    .format = NullImageFormat->vkFormat,
                    .extent = NullImageDimensions,
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = vk::SampleCountFlagBits::e1,
                    .tiling = NullImageTiling,
                    .usage = NullImageUsage,
                    .sharingMode = vk::SharingMode::eExclusive,
                    .queueFamilyIndexCount = 1,
                    .pQueueFamilyIndices = &gpu.vkQueueFamilyIndex,
                    .initialLayout = NullImageInitialLayout
                }
            )};

            auto nullTexture{std::make_shared<Texture>(gpu, std::move(vkImage), NullImageDimensions, NullImageFormat, NullImageInitialLayout, NullImageTiling, NullImageFlags, NullImageUsage)};
            nullTexture->TransitionLayout(vk::ImageLayout::eGeneral);
            nullTextureView = nullTexture->GetView(vk::ImageViewType::e2D, vk::ImageSubresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .levelCount = 1,
                .layerCount = 1,
            });
        }

        /* Render Targets + Render Target Control */
      private:
        struct RenderTarget {
            bool disabled{true}; //!< If this RT has been disabled and will be an unbound attachment instead
            IOVA iova{};
            u32 widthBytes{}; //!< The width in bytes for linear textures
            u32 layerStride{}; //!< The stride of a single layer in bytes
            bool is3d{}; //!< If the RT is 3D, this controls if the RT is 3D or layered
            GuestTexture guest{};
            std::shared_ptr<TextureView> view{};

            RenderTarget() {
                guest.dimensions = texture::Dimensions(1, 1, 1);
                guest.layerCount = 1;
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
                    FORMAT_SAME_CASE(B10G11R11, Float);
                    FORMAT_SAME_NORM_INT_FLOAT_CASE(R16G16);
                    FORMAT_SAME_CASE(R8G8B8A8, Unorm);
                    FORMAT_SAME_CASE(R8G8B8A8, Srgb);
                    FORMAT_NORM_INT_SRGB_CASE(R8G8B8X8, R8G8B8A8);
                    FORMAT_SAME_CASE(B8G8R8A8, Unorm);
                    FORMAT_SAME_CASE(B8G8R8A8, Srgb);
                    FORMAT_SAME_CASE(A2B10G10R10, Unorm);
                    FORMAT_SAME_CASE(A2B10G10R10, Uint);
                    FORMAT_SAME_INT_CASE(R32G32);
                    FORMAT_SAME_CASE(R32G32, Float);
                    FORMAT_SAME_CASE(R16G16B16A16, Float);
                    FORMAT_NORM_INT_FLOAT_CASE(R16G16B16X16, R16G16B16A16);
                    FORMAT_SAME_INT_FLOAT_CASE(R32G32B32A32);
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

            if (renderTarget.guest.format)
                renderTarget.guest.aspect = renderTarget.guest.format->vkAspect;

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
                        return format::S8UintD24Unorm;
                    case MaxwellDepthRtFormat::D24X8Unorm:
                        return format::D24UnormX8Uint;
                    case MaxwellDepthRtFormat::D24S8Unorm:
                        return format::D24UnormS8Uint;
                    case MaxwellDepthRtFormat::D32S8X24Float:
                        return format::D32FloatS8Uint;
                    default:
                        throw exception("Cannot translate the supplied depth RT format: 0x{:X}", static_cast<u32>(format));
                }
            }();

            if (depthRenderTarget.guest.format)
                depthRenderTarget.guest.aspect = depthRenderTarget.guest.format->vkAspect;

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

            if (renderTarget.is3d != mode.is3d) {
                std::swap(renderTarget.guest.dimensions.depth, renderTarget.guest.layerCount);
                renderTarget.is3d = mode.is3d;
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
            if (renderTarget.is3d)
                renderTarget.guest.dimensions.depth = mode.depthOrlayerCount;
            else
                renderTarget.guest.layerCount = mode.depthOrlayerCount;
            renderTarget.view.reset();
        }

        void SetColorRenderTargetArrayMode(size_t index, maxwell3d::RenderTargetArrayMode mode) {
            auto &renderTarget{colorRenderTargets.at(index)};
            if (mode.volume)
                throw exception("Color RT Array Volumes are not supported (with {} = {})", renderTarget.is3d ? "depth" : "layer count", mode.depthOrlayerCount);
            SetRenderTargetArrayMode(renderTarget, mode);
        }

        void SetDepthRenderTargetArrayMode(maxwell3d::RenderTargetArrayMode mode) {
            SetRenderTargetArrayMode(depthRenderTarget, mode);
        }

        void SetRenderTargetLayerStride(RenderTarget &renderTarget, u32 layerStrideLsr2) {
            renderTarget.layerStride = layerStrideLsr2 << 2;
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

            if (renderTarget.guest.baseArrayLayer > 0 || renderTarget.guest.layerCount > 1)
                renderTarget.guest.layerStride = renderTarget.layerStride; // Games can supply a layer stride that may include intentional padding which can contain additional mip layers
            else
                renderTarget.guest.layerStride = 0; // We want to explicitly reset the stride to 0 for non-array textures

            auto mappings{channelCtx.asCtx->gmmu.TranslateRange(renderTarget.iova, renderTarget.guest.GetSize())};
            renderTarget.guest.mappings.assign(mappings.begin(), mappings.end());

            renderTarget.guest.viewType = [&]() {
                if (renderTarget.is3d)
                    return vk::ImageViewType::e2DArray; // We can't render to 3D textures, so render to a 2D array view of a 3D texture (since layerCount is 1 and depth is >1 the texture manager will create the underlying texture as such)

                if (renderTarget.guest.layerCount > 1)
                    return vk::ImageViewType::e2DArray;

                return vk::ImageViewType::e2D;
            }();

            renderTarget.view = executor.AcquireTextureManager().FindOrCreate(renderTarget.guest, executor.tag);
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
            BufferView *view;

            /**
             * @brief Reads an object from the supplied offset in the constant buffer
             * @note This must only be called when the GuestBuffer is resolved correctly
             */
            template<typename T>
            T Read(CommandExecutor &pExecutor, size_t dstOffset) const {
                T object;
                ContextLock lock{pExecutor.tag, *view};
                view->Read(lock.IsFirstUsage(), []() {
                    // TODO: here we should trigger a SubmitWithFlush, however that doesn't currently work due to Read being called mid-draw and attached objects not handling this case
                    Logger::Warn("GPU dirty buffer reads for attached buffers are unimplemented");
                }, span<T>(object).template cast<u8>(), dstOffset);
                return object;
            }

            /**
             * @brief Writes a span to the supplied offset in the constant buffer
             * @note This must only be called when the GuestBuffer is resolved correctly
             */
            template<typename T>
            void Write(CommandExecutor &pExecutor, MegaBufferAllocator &megaBufferAllocator, span<T> buf, size_t dstOffset) {
                auto srcCpuBuf{buf.template cast<u8>()};

                ContextLock lock{pExecutor.tag, *view};
                view->Write(lock.IsFirstUsage(), pExecutor.cycle, []() {
                    // TODO: see Read()
                    Logger::Warn("GPU dirty buffer reads for attached buffers are unimplemented");
                }, [&megaBufferAllocator, &pExecutor, srcCpuBuf, dstOffset, &view = this->view, &lock]() {
                    pExecutor.AttachLockedBufferView(*view, std::move(lock));
                    // This will prevent any CPU accesses to backing for the duration of the usage
                    // ONLY in this specific case is it fine to access the backing buffer directly since the flag will be propagated with recreations
                    (*view)->buffer->BlockAllCpuBackingWrites();

                    auto srcGpuAllocation{megaBufferAllocator.Push(pExecutor.cycle, srcCpuBuf)};
                    pExecutor.AddOutsideRpCommand([=](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &) {
                        vk::BufferCopy copyRegion{
                            .size = srcCpuBuf.size_bytes(),
                            .srcOffset = srcGpuAllocation.offset,
                            .dstOffset = (*view)->view->offset + dstOffset
                        };
                        commandBuffer.copyBuffer(srcGpuAllocation.buffer, (*view)->buffer->GetBacking(), copyRegion);
                    });
                }, srcCpuBuf, dstOffset);
            }
        };

        ConstantBuffer constantBufferSelector{}; //!< The constant buffer selector is used to bind a constant buffer to a stage or update data in it

      public:
        void SetConstantBufferSelectorSize(u32 size) {
            constantBufferSelector.size = size;
            constantBufferSelector.view = {};
        }

        void SetConstantBufferSelectorIovaHigh(u32 high) {
            constantBufferSelector.iova.high = high;
            constantBufferSelector.view = {};
        }

        void SetConstantBufferSelectorIovaLow(u32 low) {
            constantBufferSelector.iova.low = low;
            constantBufferSelector.view = {};
        }

        /**
         * @brief Simple hashmap cache for constant buffers to avoid the constant overhead of TranslateRange and GetView that would otherwise be present
         * @note TODO: This doesn't currently evict views but that can be fixed later when we encounter a performance issue
         */
        class ConstantBufferCache {
          private:
            struct Key {
                u32 size;
                u64 iova;

                auto operator<=>(const Key &) const = default;
            };

            struct KeyHash {
                size_t operator()(const Key &entry) const noexcept {
                    size_t seed{};
                    boost::hash_combine(seed, entry.size);
                    boost::hash_combine(seed, entry.iova);

                    return seed;
                }
            };

            std::unordered_map<Key, BufferView, KeyHash> cache;

          public:
            BufferView *Lookup(u32 size, u64 iova) {
                if (auto it{cache.find({size, iova})}; it != cache.end())
                    return &it->second;

                return nullptr;
            }

            BufferView *Insert(u32 size, u64 iova, BufferView &&view) {
                return &cache.emplace(Key{size, iova}, view).first->second;
            }
        } constantBufferCache;

        ConstantBuffer *GetConstantBufferSelector() {
            if (constantBufferSelector.size == 0)
                return nullptr;
            else if (constantBufferSelector.view)
                return &constantBufferSelector;

            auto view{constantBufferCache.Lookup(constantBufferSelector.size, constantBufferSelector.iova)};
            if (!view) {
                auto mappings{channelCtx.asCtx->gmmu.TranslateRange(constantBufferSelector.iova, constantBufferSelector.size)};
                view = constantBufferCache.Insert(constantBufferSelector.size, constantBufferSelector.iova,
                          executor.AcquireBufferManager().FindOrCreate(mappings.front(), executor.tag, [this](std::shared_ptr<Buffer> buffer, ContextLock<Buffer> &&lock) {
                              executor.AttachLockedBuffer(buffer, std::move(lock));
                          })
                       );
            }

            constantBufferSelector.view = view;
            return &constantBufferSelector;
        }

        void ConstantBufferUpdate(span<u32> data, u32 offset) {
            auto constantBuffer{GetConstantBufferSelector()};
            if (constantBuffer)
                constantBuffer->Write<u32>(executor, executor.AcquireMegaBufferAllocator(), data, offset);
            else
                throw exception("Attempting to write to invalid constant buffer!");
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
            std::array<u8, MaxShaderBytecodeSize> backing; //!< The backing storage for shader bytecode in a statically allocated array
            span<u8> bytecode{}; //!< A span of the shader bytecode inside the backing storage
            std::shared_ptr<ShaderManager::ShaderProgram> program{};

            Shader(ShaderCompiler::Stage stage, bool enabled = false) : stage(stage), enabled(enabled) {}

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
                Shader{ShaderCompiler::Stage::VertexB, true}, // VertexB cannot be disabled and must be enabled by default
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
            vk::ShaderModule vkModule;

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

        ShaderCompiler::RuntimeInfo runtimeInfo{
            .convert_depth_mode = true // This is required for the default GPU register state
        };

        constexpr static size_t PipelineUniqueDescriptorTypeCount{3}; //!< The amount of unique descriptor types that may be bound to a pipeline
        constexpr static size_t PipelineDescriptorWritesReservedCount{maxwell3d::PipelineStageCount * PipelineUniqueDescriptorTypeCount}; //!< The amount of descriptors writes reserved in advance to bind a pipeline, this is not a hard limit due to the Adreno descriptor quirk
        constexpr static size_t MaxPipelineDescriptorCount{100}; //!< The maxium amount of descriptors we support being bound to a pipeline

        boost::container::static_vector<vk::DescriptorSetLayoutBinding, MaxPipelineDescriptorCount> layoutBindings;

        /**
         * @brief All state concerning the shader programs and their bindings
         * @note The `descriptorSetWrite` will have a null `dstSet` which needs to be assigned prior to usage
         */
        struct ShaderProgramState {
            boost::container::static_vector<vk::ShaderModule, maxwell3d::PipelineStageCount> shaderModules; //!< Shader modules for every pipeline stage
            boost::container::static_vector<vk::PipelineShaderStageCreateInfo, maxwell3d::PipelineStageCount> shaderStages; //!< Shader modules for every pipeline stage
            std::vector<vk::DescriptorSetLayoutBinding> descriptorSetBindings; //!< The descriptor set layout for the pipeline (Only valid when `activeShaderStagesInfoCount` is non-zero)

            struct DescriptorSetWrites {
                std::vector<vk::WriteDescriptorSet> writes; //!< The descriptor set writes for the pipeline
                std::vector<vk::DescriptorBufferInfo> bufferDescriptors; //!< The storage for buffer descriptors
                std::vector<vk::DescriptorImageInfo> imageDescriptors; //!< The storage for image descriptors

                std::vector<vk::WriteDescriptorSet> &operator*() {
                    return writes;
                }

                std::vector<vk::WriteDescriptorSet> *operator->() {
                    return &writes;
                }
            };

            DescriptorSetWrites descriptorSetWrites; //!< The writes to the descriptor set that need to be done prior to executing a pipeline
        };

        /**
         * @brief A packed handle supplied by the guest containing the index into the TIC and TSC tables
         */
        union BindlessTextureHandle {
            u32 raw;
            struct {
                u32 textureIndex : 20;
                u32 samplerIndex : 12;
            };
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

        BufferView GetSsboViewFromDescriptor(const ::Shader::StorageBufferDescriptor &descriptor, const std::array<ConstantBuffer, maxwell3d::PipelineStageConstantBufferCount> &constantBuffers) {
            struct SsboDescriptor {
                IOVA iova;
                u32 size;
            };

            auto &cbuf{constantBuffers[descriptor.cbuf_index]};
            auto ssbo{cbuf.Read<SsboDescriptor>(executor, descriptor.cbuf_offset)};

            auto mappings{channelCtx.asCtx->gmmu.TranslateRange(ssbo.iova, ssbo.size)};
            if (mappings.size() != 1)
                Logger::Warn("Multiple buffer mappings ({}) are not supported", mappings.size());

            return executor.AcquireBufferManager().FindOrCreate(mappings.front(), executor.tag, [this](std::shared_ptr<Buffer> buffer, ContextLock<Buffer> &&lock) {
                executor.AttachLockedBuffer(buffer, std::move(lock));
            });
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

                    auto readConstantBuffer{[&executor = executor, &constantBuffers = pipelineStage.constantBuffers](u32 index, u32 offset) {
                        return constantBuffers[index].Read<u32>(executor, offset);
                    }};

                    auto getTextureType{[this](u32 handle) {
                        using TicType = TextureImageControl::TextureType;
                        using ShaderType = ShaderCompiler::TextureType;
                        switch (GetTextureImageControl(BindlessTextureHandle{handle}.textureIndex).textureType) {
                            case TicType::e1D:
                                return ShaderType::Color1D;
                            case TicType::e1DArray:
                                return ShaderType::ColorArray1D;
                            case TicType::e1DBuffer:
                                return ShaderType::Buffer;

                            case TicType::e2D:
                            case TicType::e2DNoMipmap:
                                return ShaderType::Color2D;
                            case TicType::e2DArray:
                                return ShaderType::ColorArray2D;

                            case TicType::e3D:
                                return ShaderType::Color3D;

                            case TicType::eCube:
                                return ShaderType::ColorCube;
                            case TicType::eCubeArray:
                                return ShaderType::ColorArrayCube;
                        }
                    }};

                    if (!shader.invalidated && shader.program)
                        shader.invalidated |= shader.program->VerifyState(readConstantBuffer, getTextureType);

                    if (shader.invalidated) {
                        // If a shader is invalidated, we need to reparse the program (given that it has changed)

                        bool shouldParseShader{[&]() {
                            if (shader.bytecode.valid() && shader.shouldCheckSame) {
                                // A fast path to check if the shader is the same as before to avoid reparsing the shader
                                auto newIovaRanges{channelCtx.asCtx->gmmu.TranslateRange(shaderBaseIova + shader.offset, shader.bytecode.size())};
                                auto originalShader{shader.bytecode.data()};

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
                            shader.bytecode = channelCtx.asCtx->gmmu.ReadTill(shader.backing, shaderBaseIova + shader.offset, [](span<u8> data) -> std::optional<size_t> {
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
                            });

                            shader.program = gpu.shader.ParseGraphicsShader(shader.stage, shader.bytecode, shader.offset, bindlessTextureConstantBufferIndex, readConstantBuffer, getTextureType);

                            if (shader.stage != ShaderCompiler::Stage::VertexA && shader.stage != ShaderCompiler::Stage::VertexB) {
                                pipelineStage.program = shader.program;
                            } else if (shader.stage == ShaderCompiler::Stage::VertexA) {
                                auto &vertexB{shaders[maxwell3d::ShaderStage::VertexB]};

                                if (!vertexB.enabled)
                                    throw exception("Enabling VertexA without VertexB is not supported");
                                else if (!vertexB.invalidated)
                                    // If only VertexA is invalidated, we need to recombine here but we can defer it otherwise
                                    pipelineStage.program = gpu.shader.CombineVertexShaders(shader.program, vertexB.program, vertexB.bytecode);
                            } else if (shader.stage == ShaderCompiler::Stage::VertexB) {
                                auto &vertexA{shaders[maxwell3d::ShaderStage::VertexA]};

                                if (vertexA.enabled)
                                    // We need to combine the vertex shader stages if VertexA is enabled
                                    pipelineStage.program = gpu.shader.CombineVertexShaders(vertexA.program, shader.program, shader.bytecode);
                                else
                                    pipelineStage.program = shader.program;
                            }

                            pipelineStage.needsRecompile = true;
                        }

                        pipelineStage.enabled = true;
                        shader.invalidated = false;
                    }
                } else if (shader.stage != ShaderCompiler::Stage::VertexA) {
                    pipelineStage.enabled = false;
                }
            }

            ShaderProgramState::DescriptorSetWrites descriptorSetWrites{};
            auto &descriptorWrites{*descriptorSetWrites};
            descriptorWrites.reserve(PipelineDescriptorWritesReservedCount);

            auto &bufferDescriptors{descriptorSetWrites.bufferDescriptors};
            auto &imageDescriptors{descriptorSetWrites.imageDescriptors};
            size_t bufferCount{}, imageCount{};
            for (auto &pipelineStage : pipelineStages) {
                if (pipelineStage.enabled) {
                    auto &program{pipelineStage.program->program};
                    bufferCount += program.info.constant_buffer_descriptors.size() + program.info.storage_buffers_descriptors.size();
                    imageCount += program.info.texture_descriptors.size();
                }
            }
            bufferDescriptors.resize(bufferCount);
            imageDescriptors.resize(imageCount);

            layoutBindings.clear();

            runtimeInfo.previous_stage_stores.mask.set(); // First stage should always have all bits set
            ShaderCompiler::Backend::Bindings bindings{};

            size_t bufferIndex{}, imageIndex{};
            boost::container::static_vector<vk::ShaderModule, maxwell3d::PipelineStageCount> shaderModules;
            boost::container::static_vector<vk::PipelineShaderStageCreateInfo, maxwell3d::PipelineStageCount> shaderStages;

            // These parts of runtime info can only be set for the last stage that handles vertices in the current pipeline (either vertex or geometry) and should be unset for all others
            struct StashedRuntimeInfo {
                std::vector<::Shader::TransformFeedbackVarying> transformFeedbackVaryings;
                std::optional<float> fixedStatePointSize;
                bool convertDepthMode;
            } stashedRuntimeInfo{
                .fixedStatePointSize = runtimeInfo.fixed_state_point_size,
                .convertDepthMode = runtimeInfo.convert_depth_mode,
            };

            auto stashRuntimeInfo{[&]() {
                stashedRuntimeInfo.transformFeedbackVaryings = std::move(runtimeInfo.xfb_varyings);
                runtimeInfo.xfb_varyings.clear();
                runtimeInfo.fixed_state_point_size = {};
                runtimeInfo.convert_depth_mode = false;
            }};

            auto unstashRuntimeInfo{[&]() {
                runtimeInfo.xfb_varyings = std::move(stashedRuntimeInfo.transformFeedbackVaryings);
                runtimeInfo.fixed_state_point_size = stashedRuntimeInfo.fixedStatePointSize;
                runtimeInfo.convert_depth_mode = stashedRuntimeInfo.convertDepthMode;
            }};

            stashRuntimeInfo();
            bool hasFullGeometryStage{pipelineStages[maxwell3d::PipelineStage::Geometry].enabled && !pipelineStages[maxwell3d::PipelineStage::Geometry].program->program.is_geometry_passthrough};
            for (auto &pipelineStage : pipelineStages) {
                if (!pipelineStage.enabled)
                    continue;

                bool needsUnstash{(pipelineStage.vkStage == vk::ShaderStageFlagBits::eGeometry && hasFullGeometryStage) ||
                                  (pipelineStage.vkStage == vk::ShaderStageFlagBits::eVertex && !hasFullGeometryStage)};
                if (needsUnstash)
                    unstashRuntimeInfo();

                if (pipelineStage.needsRecompile || bindings.unified != pipelineStage.bindingBase || pipelineStage.previousStageStores.mask != runtimeInfo.previous_stage_stores.mask) {
                    pipelineStage.previousStageStores = runtimeInfo.previous_stage_stores;
                    pipelineStage.bindingBase = bindings.unified;
                    pipelineStage.vkModule = gpu.shader.CompileShader(runtimeInfo, pipelineStage.program, bindings);
                    pipelineStage.bindingLast = bindings.unified;
                }

                if (needsUnstash)
                    stashRuntimeInfo();

                auto &program{pipelineStage.program->program};
                runtimeInfo.previous_stage_stores = program.info.stores;
                if (program.is_geometry_passthrough)
                    runtimeInfo.previous_stage_stores.mask |= program.info.passthrough.mask;
                bindings.unified = pipelineStage.bindingLast;

                // The different descriptors types must be written in the correct order.

                u32 bindingIndex{pipelineStage.bindingBase};
                if (!program.info.constant_buffer_descriptors.empty()) {
                    descriptorWrites.push_back(vk::WriteDescriptorSet{
                        .dstBinding = bindingIndex,
                        .descriptorCount = static_cast<u32>(program.info.constant_buffer_descriptors.size()),
                        .descriptorType = vk::DescriptorType::eUniformBuffer,
                        .pBufferInfo = bufferDescriptors.data() + bufferIndex,
                    });

                    for (auto &constantBuffer : program.info.constant_buffer_descriptors) {
                        layoutBindings.push_back(vk::DescriptorSetLayoutBinding{
                            .binding = bindingIndex++,
                            .descriptorType = vk::DescriptorType::eUniformBuffer,
                            .descriptorCount = 1,
                            .stageFlags = pipelineStage.vkStage,
                        });

                        auto &view{*pipelineStage.constantBuffers[constantBuffer.index].view};
                        executor.AttachBuffer(view);
                        if (auto megaBufferAllocation{view.AcquireMegaBuffer(executor.cycle, executor.AcquireMegaBufferAllocator())}) {
                            // If the buffer is megabuffered then since we don't get out data from the underlying buffer, rather the megabuffer which stays consistent throughout a single execution, we can skip registering usage
                            bufferDescriptors[bufferIndex] = vk::DescriptorBufferInfo{
                                .buffer = megaBufferAllocation.buffer,
                                .offset = megaBufferAllocation.offset,
                                .range = view->view->size
                            };
                        } else {
                            view.RegisterUsage(executor.allocator, executor.cycle, [descriptor = bufferDescriptors.data() + bufferIndex](const Buffer::BufferViewStorage &view, const std::shared_ptr<Buffer> &buffer) {
                                *descriptor = vk::DescriptorBufferInfo{
                                    .buffer = buffer->GetBacking(),
                                    .offset = view.offset,
                                    .range = view.size,
                                };
                            });
                        }

                        bufferIndex++;
                    }
                }

                if (!program.info.storage_buffers_descriptors.empty()) {
                    descriptorWrites.push_back(vk::WriteDescriptorSet{
                        .dstBinding = bindingIndex,
                        .descriptorCount = static_cast<u32>(program.info.storage_buffers_descriptors.size()),
                        .descriptorType = vk::DescriptorType::eStorageBuffer,
                        .pBufferInfo = bufferDescriptors.data() + bufferIndex,
                    });

                    for (auto &storageBuffer : program.info.storage_buffers_descriptors) {
                        layoutBindings.push_back(vk::DescriptorSetLayoutBinding{
                            .binding = bindingIndex++,
                            .descriptorType = vk::DescriptorType::eStorageBuffer,
                            .descriptorCount = 1,
                            .stageFlags = pipelineStage.vkStage,
                        });

                        auto view{GetSsboViewFromDescriptor(storageBuffer, pipelineStage.constantBuffers)};
                        executor.AttachBuffer(view);

                        if (storageBuffer.is_written)
                            view->buffer->MarkGpuDirty();

                        view.RegisterUsage(executor.allocator, executor.cycle, [descriptor = bufferDescriptors.data() + bufferIndex++](const Buffer::BufferViewStorage &view, const std::shared_ptr<Buffer> &buffer) {
                            *descriptor = vk::DescriptorBufferInfo{
                                .buffer = buffer->GetBacking(),
                                .offset = view.offset,
                                .range = view.size,
                            };
                        });
                    }
                }

                if (!program.info.texture_buffer_descriptors.empty())
                    Logger::Warn("Found {} texture buffer descriptor", program.info.texture_buffer_descriptors.size());

                if (!program.info.image_buffer_descriptors.empty())
                    Logger::Warn("Found {} image buffer descriptor", program.info.image_buffer_descriptors.size());

                if (!program.info.texture_descriptors.empty()) {
                    if (!gpu.traits.quirks.needsIndividualTextureBindingWrites)
                        descriptorWrites.push_back(vk::WriteDescriptorSet{
                            .dstBinding = bindingIndex,
                            .descriptorCount = static_cast<u32>(program.info.texture_descriptors.size()),
                            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                            .pImageInfo = imageDescriptors.data() + imageIndex,
                        });
                    else
                        descriptorWrites.reserve(descriptorWrites.size() + program.info.texture_descriptors.size());

                    for (auto &texture : program.info.texture_descriptors) {
                        if (gpu.traits.quirks.needsIndividualTextureBindingWrites)
                            descriptorWrites.push_back(vk::WriteDescriptorSet{
                                .dstBinding = bindingIndex,
                                .descriptorCount = 1,
                                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                .pImageInfo = imageDescriptors.data() + imageIndex,
                            });

                        layoutBindings.push_back(vk::DescriptorSetLayoutBinding{
                            .binding = bindingIndex++,
                            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                            .descriptorCount = 1,
                            .stageFlags = pipelineStage.vkStage,
                        });

                        auto &constantBuffer{pipelineStage.constantBuffers[texture.cbuf_index]};
                        BindlessTextureHandle handle{constantBuffer.Read<u32>(executor, texture.cbuf_offset)};
                        if (tscIndexLinked)
                            handle.samplerIndex = handle.textureIndex;

                        auto sampler{GetSampler(handle.samplerIndex)};
                        executor.AttachDependency(sampler);

                        auto textureView{GetPoolTextureView(handle.textureIndex)};
                        executor.AttachTexture(textureView.get());

                        imageDescriptors[imageIndex++] = vk::DescriptorImageInfo{
                            .sampler = **sampler,
                            .imageView = textureView->GetView(),
                            .imageLayout = textureView->texture->layout,
                        };
                    }
                }

                if (!program.info.image_descriptors.empty())
                    Logger::Warn("Found {} image descriptor", program.info.image_descriptors.size());

                shaderModules.emplace_back(pipelineStage.vkModule);
                shaderStages.emplace_back(vk::PipelineShaderStageCreateInfo{
                    .stage = pipelineStage.vkStage,
                    .module = pipelineStage.vkModule,
                    .pName = "main",
                });
            }

            unstashRuntimeInfo();

            return {
                std::move(shaderModules),
                std::move(shaderStages),
                {layoutBindings.begin(), layoutBindings.end()},
                std::move(descriptorSetWrites),
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

            // VertexB shaders cannot be disabled in HW so ignore any attempts to disable them
            if (stage != maxwell3d::ShaderStage::VertexB) {
                shader.enabled = enabled;
                shader.invalidated = true;
            }
        }

        void SetShaderOffset(maxwell3d::ShaderStage stage, u32 offset) {
            auto &shader{shaders[stage]};
            shader.offset = offset;
            shader.invalidated = true;
        }

        void BindPipelineConstantBuffer(maxwell3d::PipelineStage stage, bool enable, u32 index) {
            auto &targetConstantBuffer{pipelineStages[stage].constantBuffers[index]};

            if (auto selector{GetConstantBufferSelector()}; selector && enable)
                targetConstantBuffer = *selector;
            else
                targetConstantBuffer = {};
        }

        /* Rasterizer State */
      private:
        vk::StructureChain<vk::PipelineRasterizationStateCreateInfo, vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT> rasterizerState;
        bool cullFaceEnabled{};
        vk::CullModeFlags cullMode{}; //!< The current cull mode regardless of it being enabled or disabled
        vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT provokingVertexState{};
        bool depthBiasPoint{}, depthBiasLine{}, depthBiasFill{};
        bool frontFaceFlip{}; //!< If the front face has to be flipped from what is supplied in SetFrontFace()

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
            if (enabled)
                rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>().cullMode = cullMode;
            else
                rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>().cullMode = {};
        }

        void SetFrontFace(maxwell3d::FrontFace face) {
            rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>().frontFace = [&]() {
                switch (face) {
                    case maxwell3d::FrontFace::Clockwise:
                        return frontFaceFlip ? vk::FrontFace::eCounterClockwise : vk::FrontFace::eClockwise;
                    case maxwell3d::FrontFace::CounterClockwise:
                        return frontFaceFlip ? vk::FrontFace::eClockwise : vk::FrontFace::eCounterClockwise;
                }
            }();
        }

        void SetFrontFaceFlipEnabled(bool enabled) {
            if (enabled != frontFaceFlip) {
                auto &face{rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>().frontFace};
                if (face == vk::FrontFace::eClockwise)
                    face = vk::FrontFace::eCounterClockwise;
                else if (face == vk::FrontFace::eCounterClockwise)
                    face = vk::FrontFace::eClockwise;

                UpdateRuntimeInformation(runtimeInfo.y_negate, enabled, maxwell3d::PipelineStage::Vertex, maxwell3d::PipelineStage::Fragment);
                frontFaceFlip = enabled;
            }
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
                if (!gpu.traits.supportsLastProvokingVertex)
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

        void SetDepthMode(maxwell3d::DepthMode mode) {
            UpdateRuntimeInformation(runtimeInfo.convert_depth_mode, mode == maxwell3d::DepthMode::MinusOneToOne, maxwell3d::PipelineStage::Vertex, maxwell3d::PipelineStage::Geometry);
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
            if (!gpu.traits.supportsLogicOp && enabled) {
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
            commonRtBlendState[index].blendEnable = enable;
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
            BufferView view;
        };
        std::array<VertexBuffer, maxwell3d::VertexBufferCount> vertexBuffers{};

        struct VertexAttribute {
            bool enabled{};
            vk::VertexInputAttributeDescription description;
        };
        std::array<VertexAttribute, maxwell3d::VertexAttributeCount> vertexAttributes{};

      public:
        bool needsQuadConversion{}; //!< Whether the current primitive topology is quads and needs conversion to triangles

      private:
        std::shared_ptr<Buffer> quadListConversionBuffer{}; //!< Index buffer used for QuadList conversion

        /**
         * @brief Retrieves an index buffer for converting a non-indexed quad list to a triangle list
         * @result A tuple containing a view over the index buffer, the index type and the index count
         */
        std::tuple<BufferView, vk::IndexType, u32> GetNonIndexedQuadConversionBuffer(u32 count) {
            vk::DeviceSize size{conversion::quads::GetRequiredBufferSize(count, vk::IndexType::eUint32)};
            if (!quadListConversionBuffer || quadListConversionBuffer->GetBackingSpan().size_bytes() < size) {
                quadListConversionBuffer = std::make_shared<Buffer>(gpu, size);
                conversion::quads::GenerateQuadListConversionBuffer(quadListConversionBuffer->GetBackingSpan().cast<u32>().data(), count);
            }
            return {quadListConversionBuffer->GetView(0, size), vk::IndexType::eUint32, conversion::quads::GetIndexCount(count)};
        }

        MegaBufferAllocator::Allocation GetIndexedQuadConversionBuffer(u32 count) {
            vk::DeviceSize size{conversion::quads::GetRequiredBufferSize(count, indexBuffer.type)};
            auto allocation{executor.AcquireMegaBufferAllocator().Allocate(executor.cycle, size)};

            ContextLock lock{executor.tag, indexBuffer.view};
            auto guestIndexBuffer{indexBuffer.view.GetReadOnlyBackingSpan(lock.IsFirstUsage(), []() {
                // TODO: see Read()
                Logger::Error("Dirty index buffer reads for attached buffers are unimplemented");
            })};
            conversion::quads::GenerateIndexedQuadConversionBuffer(allocation.region.data(), guestIndexBuffer.data(), count, indexBuffer.type);
            return allocation;
        }

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
            vertexBuffer.view = {};
        }

        void SetVertexBufferStartIovaLow(u32 index, u32 low) {
            auto &vertexBuffer{vertexBuffers[index]};
            vertexBuffer.start.low = low;
            vertexBuffer.view = {};
        }

        void SetVertexBufferEndIovaHigh(u32 index, u32 high) {
            auto &vertexBuffer{vertexBuffers[index]};
            vertexBuffer.end.high = high;
            vertexBuffer.view = {};
        }

        void SetVertexBufferEndIovaLow(u32 index, u32 low) {
            auto &vertexBuffer{vertexBuffers[index]};
            vertexBuffer.end.low = low;
            vertexBuffer.view = {};
        }

        void SetVertexBufferDivisor(u32 index, u32 divisor) {
            if (!gpu.traits.supportsVertexAttributeDivisor)
                Logger::Warn("Cannot set vertex attribute divisor without host GPU support");
            else if (divisor == 0 && !gpu.traits.supportsVertexAttributeZeroDivisor)
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

                /* 11-bit G and R, 10-bit B */
                FORMAT_CASE(e11_11_10, Float, Ufloat, eB10G11R11, Pack32);

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

        VertexBuffer *GetVertexBuffer(size_t index) {
            auto &vertexBuffer{vertexBuffers.at(index)};
            if (vertexBuffer.start > vertexBuffer.end || vertexBuffer.start == 0 || vertexBuffer.end == 0)
                return nullptr;
            else if (vertexBuffer.view)
                return &vertexBuffer;

            auto mappings{channelCtx.asCtx->gmmu.TranslateRange(vertexBuffer.start, (vertexBuffer.end + 1) - vertexBuffer.start)};
            if (mappings.size() != 1)
                Logger::Warn("Multiple buffer mappings ({}) are not supported", mappings.size());

            vertexBuffer.view = executor.AcquireBufferManager().FindOrCreate(mappings.front(), executor.tag, [this](std::shared_ptr<Buffer> buffer, ContextLock<Buffer> &&lock) {
                executor.AttachLockedBuffer(buffer, std::move(lock));
            });
            return &vertexBuffer;
        }

        /* Input Assembly */
      private:
        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState{};
        float pointSpriteSize{}; //!< The size of a point sprite to be defined in the shader

        void ValidatePrimitiveRestartState() {
            if (inputAssemblyState.primitiveRestartEnable) {
                switch (inputAssemblyState.topology) {
                    case vk::PrimitiveTopology::eLineStrip:
                    case vk::PrimitiveTopology::eTriangleStrip:
                    case vk::PrimitiveTopology::eTriangleFan:
                    case vk::PrimitiveTopology::eLineStripWithAdjacency:
                    case vk::PrimitiveTopology::eTriangleStripWithAdjacency:
                        break; // Doesn't need any extension

                    case vk::PrimitiveTopology::ePointList:
                    case vk::PrimitiveTopology::eLineList:
                    case vk::PrimitiveTopology::eTriangleList:
                    case vk::PrimitiveTopology::eLineListWithAdjacency:
                    case vk::PrimitiveTopology::eTriangleListWithAdjacency:
                        if (!gpu.traits.supportsTopologyListRestart)
                            Logger::Warn("GPU doesn't support primitive restart with list topologies");
                        break;

                    case vk::PrimitiveTopology::ePatchList:
                        if (!gpu.traits.supportsTopologyPatchListRestart)
                            Logger::Warn("GPU doesn't support primitive restart with patch list topology");
                        break;
                }
            }
        }

      public:
        void SetPrimitiveTopology(maxwell3d::PrimitiveTopology topology) {
            auto[vkTopology, shaderTopology, isQuad]{[topology]() -> std::tuple<vk::PrimitiveTopology, ShaderCompiler::InputTopology, bool> {
                using MaxwellTopology = maxwell3d::PrimitiveTopology;
                using VkTopology = vk::PrimitiveTopology;
                using ShaderTopology = ShaderCompiler::InputTopology;
                switch (topology) {
                    // @fmt:off

                    case MaxwellTopology::PointList: return {VkTopology::ePointList, ShaderTopology::Points, false};

                    case MaxwellTopology::LineList: return {VkTopology::eLineList, ShaderTopology::Lines, false};
                    case MaxwellTopology::LineStrip: return {VkTopology::eLineStrip, ShaderTopology::Lines, false};
                    case MaxwellTopology::LineListWithAdjacency: return {VkTopology::eLineListWithAdjacency, ShaderTopology::LinesAdjacency, false};
                    case MaxwellTopology::LineStripWithAdjacency: return {VkTopology::eLineStripWithAdjacency, ShaderTopology::LinesAdjacency, false};

                    case MaxwellTopology::TriangleList: return {VkTopology::eTriangleList, ShaderTopology::Triangles, false};
                    case MaxwellTopology::TriangleStrip: return {VkTopology::eTriangleStrip, ShaderTopology::Triangles, false};
                    case MaxwellTopology::TriangleFan: return {VkTopology::eTriangleFan, ShaderTopology::Triangles, false};
                    case MaxwellTopology::TriangleListWithAdjacency: return {VkTopology::eTriangleListWithAdjacency, ShaderTopology::TrianglesAdjacency, false};
                    case MaxwellTopology::TriangleStripWithAdjacency: return {VkTopology::eTriangleStripWithAdjacency, ShaderTopology::TrianglesAdjacency, false};

                    case MaxwellTopology::QuadList: return {VkTopology::eTriangleList, ShaderTopology::Triangles, true};

                    case MaxwellTopology::PatchList: return {VkTopology::ePatchList, ShaderTopology::Triangles, false};

                    // @fmt:on

                    default:
                        throw exception("Unimplemented Maxwell3D Primitive Topology: {}", maxwell3d::ToString(topology));
                }
            }()};

            inputAssemblyState.topology = vkTopology;
            needsQuadConversion = isQuad;

            if (shaderTopology == ShaderCompiler::InputTopology::Points)
                UpdateRuntimeInformation(runtimeInfo.fixed_state_point_size, std::make_optional(pointSpriteSize), maxwell3d::PipelineStage::Vertex, maxwell3d::PipelineStage::Geometry);
            else if (runtimeInfo.input_topology == ShaderCompiler::InputTopology::Points)
                UpdateRuntimeInformation(runtimeInfo.fixed_state_point_size, std::optional<float>{}, maxwell3d::PipelineStage::Vertex, maxwell3d::PipelineStage::Geometry);

            UpdateRuntimeInformation(runtimeInfo.input_topology, shaderTopology, maxwell3d::PipelineStage::Geometry);
        }

        void SetPrimitiveRestartEnabled(bool enable) {
            inputAssemblyState.primitiveRestartEnable = enable;
        }

        void SetPointSpriteSize(float size) {
            pointSpriteSize = size;
            if (runtimeInfo.input_topology == ShaderCompiler::InputTopology::Points)
                UpdateRuntimeInformation(runtimeInfo.fixed_state_point_size, std::make_optional(size), maxwell3d::PipelineStage::Vertex, maxwell3d::PipelineStage::Geometry);
        }

        /* Tessellation */
      private:
        vk::PipelineTessellationStateCreateInfo tessellationState{};
        maxwell3d::TessellationPrimitive tessellationPrimitive{};
        maxwell3d::TessellationSpacing tessellationSpacing{};
        bool isTessellationWindingClockwise{};

        bool IsTessellationModeClockwise(maxwell3d::TessellationPrimitive primitive, maxwell3d::TessellationWinding winding) {
            if (primitive == maxwell3d::TessellationPrimitive::Isoline)
                return false;

            switch (winding) {
                case maxwell3d::TessellationWinding::CounterClockwiseAndNotConnected:
                case maxwell3d::TessellationWinding::ConnectedTriangle:
                    return false;

                case maxwell3d::TessellationWinding::ClockwiseTriangle:
                case maxwell3d::TessellationWinding::ClockwiseConnectedTriangle:
                    return true;

                default:
                    throw exception("Unimplemented Maxwell3D Tessellation Winding: {}", winding);
            }
        }

      public:
        void SetTessellationPatchSize(u32 patchControlPoints) {
            tessellationState.patchControlPoints = patchControlPoints;
        }

        void SetTessellationMode(maxwell3d::TessellationPrimitive primitive, maxwell3d::TessellationSpacing spacing, maxwell3d::TessellationWinding winding) {
            if (tessellationPrimitive != primitive) {
                auto shaderPrimitive{[](maxwell3d::TessellationPrimitive primitive) -> ShaderCompiler::TessPrimitive {
                    using Primitive = maxwell3d::TessellationPrimitive;
                    using ShaderPrimitive = ShaderCompiler::TessPrimitive;
                    switch (primitive) {
                        case Primitive::Isoline:
                            return ShaderPrimitive::Isolines;
                        case Primitive::Triangle:
                            return ShaderPrimitive::Triangles;
                        case Primitive::Quad:
                            return ShaderPrimitive::Quads;

                        default:
                            throw exception("Unimplemented Maxwell3D Tessellation Primitive: {}", primitive);
                    }
                }(primitive)};

                UpdateRuntimeInformation(runtimeInfo.tess_primitive, shaderPrimitive, maxwell3d::PipelineStage::TessellationEvaluation);
                tessellationPrimitive = primitive;
            }

            if (tessellationSpacing != spacing) {
                auto shaderTessellationSpacing{[](maxwell3d::TessellationSpacing spacing) -> ShaderCompiler::TessSpacing {
                    using Spacing = maxwell3d::TessellationSpacing;
                    using ShaderSpacing = ShaderCompiler::TessSpacing;
                    switch (spacing) {
                        case Spacing::Equal:
                            return ShaderSpacing::Equal;
                        case Spacing::FractionalEven:
                            return ShaderSpacing::FractionalEven;
                        case Spacing::FractionalOdd:
                            return ShaderSpacing::FractionalOdd;

                        default:
                            throw exception("Unimplemented Maxwell3D Tessellation Spacing: {}", spacing);
                    }
                }(spacing)};

                UpdateRuntimeInformation(runtimeInfo.tess_spacing, shaderTessellationSpacing, maxwell3d::PipelineStage::TessellationEvaluation);
                tessellationSpacing = spacing;
            }

            bool isClockwise{IsTessellationModeClockwise(primitive, winding)};
            if (isTessellationWindingClockwise != isClockwise) {
                UpdateRuntimeInformation(runtimeInfo.tess_clockwise, isClockwise, maxwell3d::PipelineStage::TessellationEvaluation);
                isTessellationWindingClockwise = isClockwise;
            }
        }

        /* Index Buffer */
      private:
        struct IndexBuffer {
            IOVA start{}, end{}; //!< IOVAs covering a contiguous region in GPU AS containing the index buffer (end does not represent the true extent of the index buffers, just a maximum possible extent and is set to extremely high values which cannot be used to create a buffer)
            vk::IndexType type{};
            vk::DeviceSize viewSize{}; //!< The size of the cached view
            BufferView view{}; //!< A cached view tied to the IOVAs and size to allow for a faster lookup

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
        std::shared_ptr<TextureView> nullTextureView; //!< View used instead of a null descriptor when an empty TIC is encountered, this avoids the need for the nullDescriptor VK feature

        struct PoolTexture {
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
            if (bindlessTextureConstantBufferIndex != index) {
                bindlessTextureConstantBufferIndex = index;
                for (auto &shader : shaders) {
                    shader.invalidated = true;
                    shader.shouldCheckSame = false;
                }
            }
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
        texture::Format ConvertTicFormat(TextureImageControl::FormatWord format, bool srgb) {
            using TIC = TextureImageControl;
            #define TIC_FORMAT(fmt, compR, compG, compB, compA, srgb) \
                TIC::FormatWord{ .format = TIC::ImageFormat::fmt,          \
                                 .componentR = TIC::ImageComponent::compR, \
                                 .componentG = TIC::ImageComponent::compG, \
                                 .componentB = TIC::ImageComponent::compB, \
                                 .componentA = TIC::ImageComponent::compA, \
                                 ._pad_ = srgb }.Raw() // Reuse _pad_ to store if the texture is sRGB

            // For formats where all components are of the same type
            #define TIC_FORMAT_ST(format, component) \
                TIC_FORMAT(format, component, component, component, component, false)

            #define TIC_FORMAT_ST_SRGB(format, component) \
                TIC_FORMAT(format, component, component, component, component, true)

            #define TIC_FORMAT_CASE(ticFormat, skFormat, componentR, componentG, componentB, componentA)  \
                case TIC_FORMAT(ticFormat, componentR, componentG, componentB, componentA, false): \
                    return format::skFormat

            #define TIC_FORMAT_CASE_ST(ticFormat, skFormat, component)  \
                case TIC_FORMAT_ST(ticFormat, component): \
                    return format::skFormat ## component

            #define TIC_FORMAT_CASE_ST_SRGB(ticFormat, skFormat, component)  \
                case TIC_FORMAT_ST_SRGB(ticFormat, component): \
                    return format::skFormat ## Srgb

            #define TIC_FORMAT_CASE_NORM(ticFormat, skFormat)  \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Unorm); \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Snorm)

            #define TIC_FORMAT_CASE_INT(ticFormat, skFormat)  \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Uint); \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Sint)

            #define TIC_FORMAT_CASE_NORM_INT(ticFormat, skFormat) \
                TIC_FORMAT_CASE_NORM(ticFormat, skFormat); \
                TIC_FORMAT_CASE_INT(ticFormat, skFormat)

            #define TIC_FORMAT_CASE_NORM_INT_FLOAT(ticFormat, skFormat) \
                TIC_FORMAT_CASE_NORM_INT(ticFormat, skFormat); \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Float)

            #define TIC_FORMAT_CASE_INT_FLOAT(ticFormat, skFormat) \
                TIC_FORMAT_CASE_INT(ticFormat, skFormat); \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Float)

            // Ignore the swizzle components of the format word
            format._pad_ = srgb; // Reuse the _pad_ field to store the srgb flag
            switch ((format.Raw() & TextureImageControl::FormatWord::FormatColorComponentPadMask)) {
                TIC_FORMAT_CASE_NORM_INT(R8, R8);

                TIC_FORMAT_CASE_NORM_INT_FLOAT(R16, R16);
                TIC_FORMAT_CASE_ST(D16, D16, Unorm);
                TIC_FORMAT_CASE_NORM_INT(R8G8, R8G8);
                TIC_FORMAT_CASE_ST(B5G6R5, B5G6R5, Unorm);
                TIC_FORMAT_CASE_ST(R4G4B4A4, R4G4B4A4, Unorm);
                TIC_FORMAT_CASE_ST(A1B5G5R5, A1B5G5R5, Unorm);

                TIC_FORMAT_CASE_INT_FLOAT(R32, R32);
                TIC_FORMAT_CASE_ST(D32, D32, Float);
                TIC_FORMAT_CASE_NORM_INT_FLOAT(R16G16, R16G16);
                TIC_FORMAT_CASE(R8G24, D24UnormS8Uint, Uint, Unorm, Unorm, Unorm);
                TIC_FORMAT_CASE(S8D24, D24UnormS8Uint, Uint, Unorm, Uint, Uint);
                TIC_FORMAT_CASE(S8D24, D24UnormS8Uint, Uint, Unorm, Unorm, Unorm);
                TIC_FORMAT_CASE_ST(B10G11R11, B10G11R11, Float);
                TIC_FORMAT_CASE_NORM_INT(A8B8G8R8, A8B8G8R8);
                TIC_FORMAT_CASE_ST_SRGB(A8B8G8R8, A8B8G8R8, Unorm);
                TIC_FORMAT_CASE_NORM_INT(A2B10G10R10, A2B10G10R10);
                TIC_FORMAT_CASE_ST(E5B9G9R9, E5B9G9R9, Float);

                TIC_FORMAT_CASE_ST(BC1, BC1, Unorm);
                TIC_FORMAT_CASE_ST_SRGB(BC1, BC1, Unorm);
                TIC_FORMAT_CASE_NORM(BC4, BC4);
                TIC_FORMAT_CASE_INT_FLOAT(R32G32, R32G32);
                TIC_FORMAT_CASE(D32S8, D32FloatS8Uint, Float, Uint, Uint, Unorm);
                TIC_FORMAT_CASE(D32S8, D32FloatS8Uint, Float, Uint, Unorm, Unorm);
                TIC_FORMAT_CASE_NORM_INT_FLOAT(R16G16B16A16, R16G16B16A16);

                TIC_FORMAT_CASE_ST(Astc4x4, Astc4x4, Unorm);
                TIC_FORMAT_CASE_ST_SRGB(Astc4x4, Astc4x4, Unorm);
                TIC_FORMAT_CASE_ST(Astc5x5, Astc5x5, Unorm);
                TIC_FORMAT_CASE_ST_SRGB(Astc5x5, Astc5x5, Unorm);
                TIC_FORMAT_CASE_ST(Astc6x6, Astc6x6, Unorm);
                TIC_FORMAT_CASE_ST_SRGB(Astc6x6, Astc6x6, Unorm);
                TIC_FORMAT_CASE_ST(Astc8x8, Astc8x8, Unorm);
                TIC_FORMAT_CASE_ST_SRGB(Astc8x8, Astc8x8, Unorm);
                TIC_FORMAT_CASE_ST(Astc10x10, Astc10x10, Unorm);
                TIC_FORMAT_CASE_ST_SRGB(Astc10x10, Astc10x10, Unorm);
                TIC_FORMAT_CASE_ST(BC2, BC2, Unorm);
                TIC_FORMAT_CASE_ST_SRGB(BC2, BC2, Unorm);
                TIC_FORMAT_CASE_ST(BC3, BC3, Unorm);
                TIC_FORMAT_CASE_ST_SRGB(BC3, BC3, Unorm);
                TIC_FORMAT_CASE_NORM(BC5, BC5);
                TIC_FORMAT_CASE(Bc6HUfloat, Bc6HUfloat, Float, Float, Float, Float);
                TIC_FORMAT_CASE(Bc6HSfloat, Bc6HSfloat, Float, Float, Float, Float);
                TIC_FORMAT_CASE_ST(BC7, BC7, Unorm);
                TIC_FORMAT_CASE_ST_SRGB(BC7, BC7, Unorm);
                TIC_FORMAT_CASE_INT_FLOAT(R32G32B32A32, R32G32B32A32);

                default:
                    Logger::Error("Cannot translate TIC format: 0x{:X}", static_cast<u32>(format.Raw()));
                    return {};
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

        vk::ComponentMapping ConvertTicSwizzleMapping(TextureImageControl::FormatWord format, vk::ComponentMapping swizzleMapping) {
            auto convertComponentSwizzle{[swizzleMapping](TextureImageControl::ImageSwizzle swizzle) {
                switch (swizzle) {
                    case TextureImageControl::ImageSwizzle::R:
                        return swizzleMapping.r;
                    case TextureImageControl::ImageSwizzle::G:
                        return swizzleMapping.g;
                    case TextureImageControl::ImageSwizzle::B:
                        return swizzleMapping.b;
                    case TextureImageControl::ImageSwizzle::A:
                        return swizzleMapping.a;
                    case TextureImageControl::ImageSwizzle::Zero:
                        return vk::ComponentSwizzle::eZero;
                    case TextureImageControl::ImageSwizzle::OneFloat:
                    case TextureImageControl::ImageSwizzle::OneInt:
                        return vk::ComponentSwizzle::eOne;
                    default:
                        throw exception("Invalid swizzle: {:X}", static_cast<u32>(swizzle));
                }
            }};

            return vk::ComponentMapping{
                .r = convertComponentSwizzle(format.swizzleX),
                .g = convertComponentSwizzle(format.swizzleY),
                .b = convertComponentSwizzle(format.swizzleZ),
                .a = convertComponentSwizzle(format.swizzleW)
            };
        }

        const TextureImageControl &GetTextureImageControl(u32 index) {
            if (!texturePool.imageControls.valid()) {
                auto mappings{channelCtx.asCtx->gmmu.TranslateRange(texturePool.iova, texturePool.maximumIndex * sizeof(TextureImageControl))};
                if (mappings.size() != 1)
                    throw exception("Texture pool mapping count is unexpected: {}", mappings.size());
                texturePool.imageControls = mappings.front().cast<TextureImageControl>();
            }

            return texturePool.imageControls[index];
        }

        std::shared_ptr<TextureView> GetPoolTextureView(u32 index) {
            auto &textureControl{GetTextureImageControl(index)};
            auto textureIt{texturePool.textures.insert({textureControl, {}})};
            auto &poolTexture{textureIt.first->second};
            if (textureIt.second) {
                // If the entry didn't exist prior then we need to convert the TIC to a GuestTexture
                auto &guest{poolTexture.guest};
                if (auto format{ConvertTicFormat(textureControl.formatWord, textureControl.isSrgb)}) {
                    guest.format = format;
                } else {
                    poolTexture.view = nullTextureView;
                    return nullTextureView;
                }

                guest.aspect = guest.format->Aspect(textureControl.formatWord.swizzleX == TextureImageControl::ImageSwizzle::R);
                guest.swizzle = ConvertTicSwizzleMapping(textureControl.formatWord, guest.format->swizzleMapping);

                constexpr size_t CubeFaceCount{6}; //!< The amount of faces of a cube

                guest.baseArrayLayer = static_cast<u16>(textureControl.BaseLayer());
                guest.dimensions = texture::Dimensions(textureControl.widthMinusOne + 1, textureControl.heightMinusOne + 1, 1);
                u16 depth{static_cast<u16>(textureControl.depthMinusOne + 1)};

                guest.mipLevelCount = textureControl.mipMaxLevels + 1;
                guest.viewMipBase = textureControl.viewConfig.mipMinLevel;
                guest.viewMipCount = textureControl.viewConfig.mipMaxLevel - textureControl.viewConfig.mipMinLevel + 1;

                using TicType = TextureImageControl::TextureType;
                switch (textureControl.textureType) {
                    case TicType::e1D:
                        guest.viewType = vk::ImageViewType::e1D;
                        guest.layerCount = 1;
                        break;
                    case TicType::e1DArray:
                        guest.viewType = vk::ImageViewType::e1DArray;
                        guest.layerCount = depth;
                        break;
                    case TicType::e1DBuffer:
                        throw exception("1D Buffers are not supported");

                    case TicType::e2DNoMipmap:
                        guest.mipLevelCount = 1;
                        guest.viewMipBase = 0;
                        guest.viewMipCount = 1;
                    case TicType::e2D:
                        guest.viewType = vk::ImageViewType::e2D;
                        guest.layerCount = 1;
                        break;
                    case TicType::e2DArray:
                        guest.viewType = vk::ImageViewType::e2DArray;
                        guest.layerCount = depth;
                        break;

                    case TicType::e3D:
                        guest.viewType = vk::ImageViewType::e3D;
                        guest.layerCount = 1;
                        guest.dimensions.depth = depth;
                        break;

                    case TicType::eCube:
                        guest.viewType = vk::ImageViewType::eCube;
                        guest.layerCount = CubeFaceCount;
                        break;
                    case TicType::eCubeArray:
                        guest.viewType = vk::ImageViewType::eCubeArray;
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

                auto mappings{channelCtx.asCtx->gmmu.TranslateRange(textureControl.Iova(), guest.GetSize())};
                guest.mappings.assign(mappings.begin(), mappings.end());
            } else if (auto textureView{poolTexture.view.lock()}; textureView != nullptr) {
                // If the entry already exists and the view is still valid then we return it directly
                return textureView;
            }

            auto textureView{executor.AcquireTextureManager().FindOrCreate(poolTexture.guest, executor.tag)};
            poolTexture.view = textureView;
            return textureView;
        }

        /* Samplers */
      private:
        bool tscIndexLinked{}; //!< If the TSC index in bindless texture handles is the same as the TIC index or if it's independent from the TIC index

        struct Sampler : public vk::raii::Sampler {
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

        void SetTscIndexLinked(bool isTscIndexLinked) {
            tscIndexLinked = isTscIndexLinked;
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

                // See https://github.com/yuzu-emu/yuzu/blob/5af06d14337a61d9ed1093079d13f68cbb1f5451/src/video_core/renderer_vulkan/maxwell_to_vk.cpp#L35
                case TscFilter::None: return VkMode::eNearest;
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
                auto mappings{channelCtx.asCtx->gmmu.TranslateRange(samplerPool.iova, (tscIndexLinked ? texturePool.maximumIndex : samplerPool.maximumIndex) * sizeof(TextureSamplerControl))};
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
                if (vkMode == vk::SamplerAddressMode::eMirrorClampToEdge && !gpu.traits.supportsSamplerMirrorClampToEdge) [[unlikely]] {
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
                    .anisotropyEnable = gpu.traits.supportsAnisotropicFiltering && maxAnisotropy > 1.0f,
                    .maxAnisotropy = maxAnisotropy,
                    .compareEnable = samplerControl.depthCompareEnable,
                    .compareOp = ConvertSamplerCompareOp(samplerControl.depthCompareOp),
                    .minLod = samplerControl.mipFilter == TextureSamplerControl::MipFilter::None ? 0.0f : samplerControl.MinLodClamp(),
                    .maxLod = samplerControl.mipFilter == TextureSamplerControl::MipFilter::None ? 0.25f : samplerControl.MaxLodClamp(),
                    .unnormalizedCoordinates = false,
                }, vk::SamplerReductionModeCreateInfoEXT{
                    .reductionMode = ConvertSamplerReductionFilter(samplerControl.reductionFilter),
                }, vk::SamplerCustomBorderColorCreateInfoEXT{
                    .customBorderColor.float32 = {{samplerControl.borderColorR, samplerControl.borderColorG, samplerControl.borderColorB, samplerControl.borderColorA}},
                    .format = vk::Format::eUndefined,
                },
            };

            if (!gpu.traits.supportsSamplerReductionMode)
                samplerInfo.unlink<vk::SamplerReductionModeCreateInfoEXT>();

            vk::BorderColor &borderColor{samplerInfo.get<vk::SamplerCreateInfo>().borderColor};
            if (gpu.traits.supportsCustomBorderColor) {
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
            indexBuffer.view = {};
        }

        void SetIndexBufferStartIovaLow(u32 low) {
            indexBuffer.start.low = low;
            indexBuffer.view = {};
        }

        void SetIndexBufferEndIovaHigh(u32 high) {
            indexBuffer.end.high = high;
            indexBuffer.view = {};
        }

        void SetIndexBufferEndIovaLow(u32 low) {
            indexBuffer.end.low = low;
            indexBuffer.view = {};
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

            if (indexBuffer.type == vk::IndexType::eUint8EXT && !gpu.traits.supportsUint8Indices)
                throw exception("Cannot use U8 index buffer without host GPU support");

            indexBuffer.view = {};
        }

        BufferView GetIndexBuffer(u32 elementCount) {
            auto size{indexBuffer.GetIndexBufferSize(elementCount)};
            if (indexBuffer.start > indexBuffer.end || indexBuffer.start == 0 || indexBuffer.end == 0 || size == 0)
                return nullptr;
            else if (indexBuffer.view && size == indexBuffer.viewSize)
                return indexBuffer.view;

            auto mappings{channelCtx.asCtx->gmmu.TranslateRange(indexBuffer.start, size)};
            if (mappings.size() != 1)
                Logger::Warn("Multiple buffer mappings ({}) are not supported", mappings.size());

            auto mapping{mappings.front()};
            indexBuffer.view = executor.AcquireBufferManager().FindOrCreate(span<u8>(mapping.data(), size), executor.tag, [this](std::shared_ptr<Buffer> buffer, ContextLock<Buffer> &&lock) {
                executor.AttachLockedBuffer(buffer, std::move(lock));
            });
            indexBuffer.viewSize = size;
            return indexBuffer.view;
        }

        /* Depth */
        vk::PipelineDepthStencilStateCreateInfo depthState{};
        bool twoSideStencilEnabled{}; //!< If both sides of the stencil have different state
        vk::StencilOpState stencilBack{}; //!< The back of the stencil when two-side stencil is enabled

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

        void SetStencilTwoSideEnabled(bool enabled) {
            if (twoSideStencilEnabled == enabled) {
                if (enabled)
                    depthState.back = stencilBack;
                else
                    depthState.back = depthState.front;
                twoSideStencilEnabled = enabled;
            }
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
            if (!twoSideStencilEnabled)
                depthState.back.failOp = depthState.front.failOp;
        }

        void SetStencilBackFailOp(maxwell3d::StencilOp op) {
            stencilBack.failOp = ConvertStencilOp(op);
            if (twoSideStencilEnabled)
                depthState.back.failOp = stencilBack.failOp;
        }

        void SetStencilFrontPassOp(maxwell3d::StencilOp op) {
            depthState.front.passOp = ConvertStencilOp(op);
            if (!twoSideStencilEnabled)
                depthState.back.passOp = depthState.front.passOp;
        }

        void SetStencilBackPassOp(maxwell3d::StencilOp op) {
            stencilBack.passOp = ConvertStencilOp(op);
            if (twoSideStencilEnabled)
                depthState.back.passOp = stencilBack.passOp;
        }

        void SetStencilFrontDepthFailOp(maxwell3d::StencilOp op) {
            depthState.front.depthFailOp = ConvertStencilOp(op);
            if (!twoSideStencilEnabled)
                depthState.back.depthFailOp = depthState.front.depthFailOp;
        }

        void SetStencilBackDepthFailOp(maxwell3d::StencilOp op) {
            stencilBack.depthFailOp = ConvertStencilOp(op);
            if (twoSideStencilEnabled)
                depthState.back.depthFailOp = stencilBack.depthFailOp;
        }

        void SetStencilFrontCompareOp(maxwell3d::CompareOp op) {
            depthState.front.compareOp = ConvertCompareOp(op);
            if (!twoSideStencilEnabled)
                depthState.back.compareOp = depthState.front.compareOp;
        }

        void SetStencilBackCompareOp(maxwell3d::CompareOp op) {
            stencilBack.compareOp = ConvertCompareOp(op);
            if (twoSideStencilEnabled)
                depthState.back.compareOp = stencilBack.compareOp;
        }

        void SetStencilFrontCompareMask(u32 mask) {
            depthState.front.compareMask = mask;
            if (!twoSideStencilEnabled)
                depthState.back.compareMask = depthState.front.compareMask;
        }

        void SetStencilBackCompareMask(u32 mask) {
            stencilBack.compareMask = mask;
            if (twoSideStencilEnabled)
                depthState.back.compareMask = stencilBack.compareMask;
        }

        void SetStencilFrontWriteMask(u32 mask) {
            depthState.front.writeMask = mask;
            if (!twoSideStencilEnabled)
                depthState.back.writeMask = depthState.front.writeMask;
        }

        void SetStencilBackWriteMask(u32 mask) {
            stencilBack.writeMask = mask;
            if (twoSideStencilEnabled)
                depthState.back.writeMask = stencilBack.writeMask;
        }

        void SetStencilFrontReference(u32 reference) {
            depthState.front.reference = reference;
            if (!twoSideStencilEnabled)
                depthState.back.reference = depthState.front.reference;
        }

        void SetStencilBackReference(u32 reference) {
            stencilBack.reference = reference;
            if (twoSideStencilEnabled)
                depthState.back.reference = stencilBack.reference;
        }

        /* Multisampling */
        vk::PipelineMultisampleStateCreateInfo multisampleState{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
        };

        /* Transform Feedback */
      private:
        bool transformFeedbackEnabled{};

        struct TransformFeedbackBuffer {
            IOVA iova;
            u32 size;
            u32 offset;
            BufferView view;

            u32 stride;
            u32 varyingCount;
            std::array<u8, maxwell3d::TransformFeedbackVaryingCount> varyings;
        };
        std::array<TransformFeedbackBuffer, maxwell3d::TransformFeedbackBufferCount> transformFeedbackBuffers{};

        bool transformFeedbackVaryingsDirty;

        struct TransformFeedbackBufferResult {
            BufferView view;
            bool wasCached;
        };

        TransformFeedbackBufferResult GetTransformFeedbackBuffer(size_t idx) {
            auto &buffer{transformFeedbackBuffers[idx]};

            if (!buffer.iova || !buffer.size)
                return {nullptr, false};
            else if (buffer.view)
                return {buffer.view, true};

            auto mappings{channelCtx.asCtx->gmmu.TranslateRange(buffer.offset + buffer.iova, buffer.size)};
            if (mappings.size() != 1)
                Logger::Warn("Multiple buffer mappings ({}) are not supported", mappings.size());

            auto mapping{mappings.front()};
            buffer.view = executor.AcquireBufferManager().FindOrCreate(span<u8>(mapping.data(), buffer.size), executor.tag, [this](std::shared_ptr<Buffer> buffer, ContextLock<Buffer> &&lock) {
                executor.AttachLockedBuffer(buffer, std::move(lock));
            });

            return {buffer.view, false};
        }

        void FillTransformFeedbackVaryingState() {
            if (!transformFeedbackVaryingsDirty)
                return;

            runtimeInfo.xfb_varyings.clear();

            if (!transformFeedbackEnabled)
                return;

            // Will be indexed by a u8 so allocate just enough space
            runtimeInfo.xfb_varyings.resize(256);
            for (u32 i{}; i < maxwell3d::TransformFeedbackBufferCount; i++) {
                const auto &buffer{transformFeedbackBuffers[i]};

                for (u32 k{}; k < buffer.varyingCount; k++) {
                    // TODO: We could merge multiple component accesses from the same attribute into one varying
                    u8 attributeIndex{buffer.varyings[k]};
                    runtimeInfo.xfb_varyings[attributeIndex] = {
                        .buffer = i,
                        .offset = k * 4,
                        .stride = buffer.stride,
                        .components = 1,
                    };
                }
            }

            transformFeedbackVaryingsDirty = false;
        }

        void InvalidateTransformFeedbackVaryings() {
            transformFeedbackVaryingsDirty = true;

            pipelineStages[maxwell3d::PipelineStage::Vertex].needsRecompile = true;
            pipelineStages[maxwell3d::PipelineStage::Geometry].needsRecompile = true;
        }

      public:
        void SetTransformFeedbackEnabled(bool enable) {
            transformFeedbackEnabled = enable;

            if (enable && !gpu.traits.supportsTransformFeedback)
                Logger::Warn("Transform feedback used without host GPU support!");

            InvalidateTransformFeedbackVaryings();
        }

        void SetTransformFeedbackBufferEnabled(size_t idx, bool enabled) {
            if (!enabled)
                transformFeedbackBuffers[idx].iova = {};
        }

        void SetTransformFeedbackBufferIovaHigh(size_t idx, u32 high) {
            auto &buffer{transformFeedbackBuffers[idx]};
            buffer.iova.high = high;
            buffer.view = {};
        };

        void SetTransformFeedbackBufferIovaLow(size_t idx, u32 low) {
            auto &buffer{transformFeedbackBuffers[idx]};
            buffer.iova.low = low;
            buffer.view = {};
        }

        void SetTransformFeedbackBufferSize(size_t idx, u32 size) {
            auto &buffer{transformFeedbackBuffers[idx]};
            buffer.size = size;
            buffer.view = {};
        }

        void SetTransformFeedbackBufferOffset(size_t idx, u32 offset) {
            auto &buffer{transformFeedbackBuffers[idx]};
            buffer.offset = offset;
            buffer.view = {};
        }

        void SetTransformFeedbackBufferStride(size_t idx, u32 stride) {
            auto &buffer{transformFeedbackBuffers[idx]};
            buffer.stride = stride;

            InvalidateTransformFeedbackVaryings();
        }

        void SetTransformFeedbackBufferVaryingCount(size_t idx, u32 varyingCount) {
            auto &buffer{transformFeedbackBuffers[idx]};
            buffer.varyingCount = varyingCount;

            InvalidateTransformFeedbackVaryings();
        }

        void SetTransformFeedbackBufferVarying(size_t bufIdx, size_t varIdx, u32 value) {
            auto &buffer{transformFeedbackBuffers[bufIdx]};

            span(buffer.varyings).cast<u32>()[varIdx] = value;

            InvalidateTransformFeedbackVaryings();
        }

        /* Draws */
      public:
        template<bool IsIndexed>
        void Draw(u32 count, u32 first, u32 instanceCount = 1, i32 vertexOffset = 0) {
            ValidatePrimitiveRestartState();

            // Index Buffer Setup
            struct BoundIndexBuffer {
                vk::Buffer handle{};
                vk::DeviceSize offset{};
                vk::IndexType type{};
            };

            std::shared_ptr<BoundIndexBuffer> boundIndexBuffer{};
            if constexpr (IsIndexed) {
                auto indexBufferView{GetIndexBuffer(count)};
                boundIndexBuffer = std::allocate_shared<BoundIndexBuffer, LinearAllocator<BoundIndexBuffer>>(executor.allocator);
                boundIndexBuffer->type = indexBuffer.type;

                if (needsQuadConversion) {
                    auto allocation{GetIndexedQuadConversionBuffer(count)};
                    boundIndexBuffer->handle = allocation.buffer;
                    boundIndexBuffer->offset = allocation.offset;
                    count = conversion::quads::GetIndexCount(count);
                } else {
                    executor.AttachBuffer(indexBufferView);

                    boundIndexBuffer->type = indexBuffer.type;
                    if (auto megaBufferAllocation{indexBufferView.AcquireMegaBuffer(executor.cycle, executor.AcquireMegaBufferAllocator())}) {
                        // If the buffer is megabuffered then since we don't get out data from the underlying buffer, rather the megabuffer which stays consistent throughout a single execution, we can skip registering usage
                        boundIndexBuffer->handle = megaBufferAllocation.buffer;
                        boundIndexBuffer->offset = megaBufferAllocation.offset;
                    } else {
                        indexBufferView.RegisterUsage(executor.allocator, executor.cycle, [=](const Buffer::BufferViewStorage &view, const std::shared_ptr<Buffer> &buffer) {
                            boundIndexBuffer->handle = buffer->GetBacking();
                            boundIndexBuffer->offset = view.offset;
                        });
                    }
                }
            } else if (needsQuadConversion) {
                // Convert the guest-supplied quad list to an indexed triangle list
                auto[bufferView, indexType, indexCount]{GetNonIndexedQuadConversionBuffer(count)};
                executor.AttachBuffer(bufferView);

                count = indexCount;
                boundIndexBuffer = std::make_shared<BoundIndexBuffer>();
                boundIndexBuffer->type = indexType;
                boundIndexBuffer->handle = bufferView->buffer->GetBacking();
                boundIndexBuffer->offset = bufferView->view->offset;
            }

            // Vertex Buffer Setup
            struct BoundVertexBuffers {
                std::array<vk::Buffer, maxwell3d::VertexBufferCount> handles{};
                std::array<vk::DeviceSize, maxwell3d::VertexBufferCount> offsets{};
            };
            auto boundVertexBuffers{std::allocate_shared<BoundVertexBuffers, LinearAllocator<BoundVertexBuffers>>(executor.allocator)};

            boost::container::static_vector<vk::VertexInputBindingDescription, maxwell3d::VertexBufferCount> vertexBindingDescriptions{};
            boost::container::static_vector<vk::VertexInputBindingDivisorDescriptionEXT, maxwell3d::VertexBufferCount> vertexBindingDivisorsDescriptions{};

            for (u32 index{}; index < maxwell3d::VertexBufferCount; index++) {
                auto vertexBuffer{GetVertexBuffer(index)};
                if (vertexBuffer) {
                    vertexBindingDescriptions.push_back(vertexBuffer->bindingDescription);
                    if (vertexBuffer->bindingDescription.inputRate == vk::VertexInputRate::eInstance)
                        vertexBindingDivisorsDescriptions.push_back(vertexBuffer->bindingDivisorDescription);

                    auto &vertexBufferView{vertexBuffer->view};
                    executor.AttachBuffer(vertexBufferView);
                    if (auto megaBufferAllocation{vertexBufferView.AcquireMegaBuffer(executor.cycle, executor.AcquireMegaBufferAllocator())}) {
                        // If the buffer is megabuffered then since we don't get out data from the underlying buffer, rather the megabuffer which stays consistent throughout a single execution, we can skip registering usage
                        boundVertexBuffers->handles[index] = megaBufferAllocation.buffer;
                        boundVertexBuffers->offsets[index] = megaBufferAllocation.offset;
                    } else {
                        vertexBufferView.RegisterUsage(executor.allocator, executor.cycle, [handle = boundVertexBuffers->handles.data() + index, offset = boundVertexBuffers->offsets.data() + index](const Buffer::BufferViewStorage &view, const std::shared_ptr<Buffer> &buffer) {
                            *handle = buffer->GetBacking();
                            *offset = view.offset;
                        });
                    }
                }
            }

            // Vertex Attribute Setup
            boost::container::static_vector<vk::VertexInputAttributeDescription, maxwell3d::VertexAttributeCount> vertexAttributesDescriptions{};
            for (auto &vertexAttribute : vertexAttributes)
                if (vertexAttribute.enabled)
                    vertexAttributesDescriptions.push_back(vertexAttribute.description);

            struct BoundTransformFeedbackBuffers {
                std::array<vk::Buffer, maxwell3d::TransformFeedbackBufferCount> handles{};
                std::array<vk::DeviceSize, maxwell3d::TransformFeedbackBufferCount> offsets{};
                std::array<vk::DeviceSize, maxwell3d::TransformFeedbackBufferCount> sizes{};
            };

            std::shared_ptr<BoundTransformFeedbackBuffers> boundTransformFeedbackBuffers{};

            if (transformFeedbackEnabled) {
                boundTransformFeedbackBuffers = std::allocate_shared<BoundTransformFeedbackBuffers, LinearAllocator<BoundVertexBuffers>>(executor.allocator);
                for (size_t i{}; i < maxwell3d::TransformFeedbackBufferCount; i++) {
                    if (auto result{GetTransformFeedbackBuffer(i)}; result.view) {
                        auto &view{result.view};
                        executor.AttachBuffer(view);
                        view->buffer->MarkGpuDirty();
                        if (!result.wasCached) {
                            boundTransformFeedbackBuffers->sizes[i] = view->view->size;
                            view.RegisterUsage(executor.allocator, executor.cycle, [handle = boundTransformFeedbackBuffers->handles.data() + i, offset = boundTransformFeedbackBuffers->offsets.data() + i](const Buffer::BufferViewStorage &view, const std::shared_ptr<Buffer> &buffer) {
                                *handle = buffer->GetBacking();
                                *offset = view.offset;
                            });
                        }
                    }
                }
            }

            FillTransformFeedbackVaryingState();

            // Color Render Target + Blending Setup
            boost::container::static_vector<TextureView *, maxwell3d::RenderTargetCount> activeColorRenderTargets;
            for (u32 index{}; index < maxwell3d::RenderTargetCount; index++) {
                auto renderTarget{GetColorRenderTarget(index)};
                if (renderTarget) {
                    executor.AttachTexture(renderTarget);
                    activeColorRenderTargets.push_back(renderTarget);
                }
            }

            blendState.attachmentCount = static_cast<u32>(activeColorRenderTargets.size());

            // Depth/Stencil Render Target Setup
            auto depthRenderTargetView{GetDepthRenderTarget()};
            if (depthRenderTargetView)
                executor.AttachTexture(depthRenderTargetView);

            // Pipeline Creation
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

            if (!gpu.traits.supportsVertexAttributeDivisor || vertexBindingDivisorsDescriptions.empty())
                vertexState.unlink<vk::PipelineVertexInputDivisorStateCreateInfoEXT>();

            bool multiViewport{gpu.traits.supportsMultipleViewports};
            vk::PipelineViewportStateCreateInfo viewportState{
                .pViewports = viewports.data(),
                .viewportCount = static_cast<u32>(multiViewport ? maxwell3d::ViewportCount : 1),
                .pScissors = scissors.data(),
                .scissorCount = static_cast<u32>(multiViewport ? maxwell3d::ViewportCount : 1),
            };

            auto programState{CompileShaderProgramState()};
            auto compiledPipeline{gpu.graphicsPipelineCache.GetCompiledPipeline(cache::GraphicsPipelineCache::PipelineState{
                .shaderStages = programState.shaderStages,
                .vertexState = vertexState,
                .inputAssemblyState = inputAssemblyState,
                .tessellationState = tessellationState,
                .viewportState = viewportState,
                .rasterizationState = rasterizerState,
                .multisampleState = multisampleState,
                .depthStencilState = depthState,
                .colorBlendState = blendState,
                .colorAttachments = activeColorRenderTargets,
                .depthStencilAttachment = depthRenderTargetView,
            }, programState.descriptorSetBindings)};

            // Descriptor Set Binding + Update Setup
            struct DrawStorage {
                ShaderProgramState::DescriptorSetWrites descriptorSetWrites;
                DescriptorAllocator::ActiveDescriptorSet descriptorSet;

                DrawStorage(ShaderProgramState::DescriptorSetWrites &&descriptorSetWrites, DescriptorAllocator::ActiveDescriptorSet &&descriptorSet) : descriptorSetWrites{std::move(descriptorSetWrites)}, descriptorSet{std::move(descriptorSet)} {}
            };

            std::shared_ptr<DrawStorage> drawStorage{};
            if (!programState.descriptorSetWrites->empty()) {
                drawStorage = std::make_shared<DrawStorage>(std::move(programState.descriptorSetWrites), gpu.descriptor.AllocateSet(compiledPipeline.descriptorSetLayout));
                // We can't update the descriptor set here as the bindings might be retroactively updated by future draws
                executor.AttachDependency(drawStorage);
            }

            vk::Rect2D renderArea{depthRenderTargetView ? vk::Rect2D{.extent = depthRenderTargetView->texture->dimensions} : vk::Rect2D{.extent = {std::numeric_limits<u32>::max(), std::numeric_limits<u32>::max()}}};

            for (auto &colorRt : activeColorRenderTargets) {
                renderArea.extent.width = std::min(renderArea.extent.width, colorRt->texture->dimensions.width);
                renderArea.extent.height = std::min(renderArea.extent.height, colorRt->texture->dimensions.height);
            }

            // Submit Draw
            executor.AddSubpass([=, drawStorage = std::move(drawStorage), pipelineLayout = compiledPipeline.pipelineLayout, pipeline = compiledPipeline.pipeline, transformFeedbackEnabled = transformFeedbackEnabled](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle, GPU &, vk::RenderPass renderPass, u32 subpassIndex) mutable {
                auto &vertexBufferHandles{boundVertexBuffers->handles};
                for (u32 bindingIndex{}; bindingIndex != vertexBufferHandles.size(); bindingIndex++) {
                    // We need to bind all non-null vertex buffers while skipping any null ones
                    if (vertexBufferHandles[bindingIndex]) {
                        u32 bindingEndIndex{bindingIndex + 1};
                        while (bindingEndIndex < vertexBufferHandles.size() && vertexBufferHandles[bindingEndIndex])
                            bindingEndIndex++;

                        u32 bindingCount{bindingEndIndex - bindingIndex};
                        commandBuffer.bindVertexBuffers(bindingIndex, span(vertexBufferHandles.data() + bindingIndex, bindingCount), span(boundVertexBuffers->offsets.data() + bindingIndex, bindingCount));
                    }
                }

                if (drawStorage) {
                    vk::DescriptorSet descriptorSet{*drawStorage->descriptorSet};
                    for (auto &descriptorSetWrite : *drawStorage->descriptorSetWrites)
                        descriptorSetWrite.dstSet = descriptorSet;
                    gpu.vkDevice.updateDescriptorSets(*drawStorage->descriptorSetWrites, nullptr);

                    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, descriptorSet, nullptr);
                }

                commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

                if (transformFeedbackEnabled) {
                    for (u32 i{}; i < maxwell3d::TransformFeedbackBufferCount; i++)
                        if (boundTransformFeedbackBuffers->handles[i])
                            commandBuffer.bindTransformFeedbackBuffersEXT(i, span(boundTransformFeedbackBuffers->handles).subspan(i, 1), span(boundTransformFeedbackBuffers->offsets).subspan(i, 1), span(boundTransformFeedbackBuffers->sizes).subspan(i, 1));

                    commandBuffer.beginTransformFeedbackEXT(0, {}, {});
                }

                if (IsIndexed || boundIndexBuffer) {
                    commandBuffer.bindIndexBuffer(boundIndexBuffer->handle, boundIndexBuffer->offset, boundIndexBuffer->type);
                    commandBuffer.drawIndexed(count, instanceCount, first, vertexOffset, 0);
                } else {
                    commandBuffer.draw(count, instanceCount, first, 0);
                }

                if (transformFeedbackEnabled)
                    commandBuffer.endTransformFeedbackEXT(0, {}, {});
            }, renderArea, {}, activeColorRenderTargets, depthRenderTargetView, !gpu.traits.quirks.relaxedRenderPassCompatibility);
        }

        void Draw(u32 vertexCount, u32 firstVertex, u32 instanceCount) {
            Draw<false>(vertexCount, firstVertex, instanceCount);
        }

        void DrawIndexed(u32 indexCount, u32 firstIndex, u32 instanceCount, i32 vertexOffset) {
            Draw<true>(indexCount, firstIndex, instanceCount, vertexOffset);
        }
    };
}
