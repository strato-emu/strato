// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <gpu/texture/format.h>
#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include <soc/gm20b/engines/maxwell/types.h>

#include "command_executor.h"

namespace skyline::gpu::interconnect {
    namespace maxwell3d = soc::gm20b::engine::maxwell3d::type;

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
        }

        /* Render Targets + Render Target Control */
      private:
        struct RenderTarget {
            bool disabled{true}; //!< If this RT has been disabled and will be an unbound attachment instead
            union {
                u64 gpuAddress;
                struct {
                    u32 gpuAddressLow;
                    u32 gpuAddressHigh;
                };
            };
            u32 widthBytes; //!< The width in bytes for linear textures
            GuestTexture guest;
            std::optional<TextureView> view;

            RenderTarget() {
                guest.dimensions = texture::Dimensions(1, 1, 1); // We want the depth to be 1 by default (It cannot be set by the application)
            }
        };

        std::array<RenderTarget, maxwell3d::RenderTargetCount> renderTargets{}; //!< The target textures to render into as color attachments
        maxwell3d::RenderTargetControl renderTargetControl{};

      public:
        void SetRenderTargetAddressHigh(size_t index, u32 high) {
            auto &renderTarget{renderTargets.at(index)};
            renderTarget.gpuAddressHigh = high;
            renderTarget.guest.mappings.clear();
            renderTarget.view.reset();
        }

        void SetRenderTargetAddressLow(size_t index, u32 low) {
            auto &renderTarget{renderTargets.at(index)};
            renderTarget.gpuAddressLow = low;
            renderTarget.guest.mappings.clear();
            renderTarget.view.reset();
        }

        void SetRenderTargetWidth(size_t index, u32 value) {
            auto &renderTarget{renderTargets.at(index)};
            renderTarget.widthBytes = value;
            if (renderTarget.guest.tileConfig.mode == texture::TileMode::Linear && renderTarget.guest.format)
                value /= renderTarget.guest.format->bpb; // Width is in bytes rather than format units for linear textures
            renderTarget.guest.dimensions.width = value;
            renderTarget.view.reset();
        }

        void SetRenderTargetHeight(size_t index, u32 value) {
            auto &renderTarget{renderTargets.at(index)};
            renderTarget.guest.dimensions.height = value;
            renderTarget.view.reset();
        }

        void SetRenderTargetFormat(size_t index, maxwell3d::RenderTarget::ColorFormat format) {
            auto &renderTarget{renderTargets.at(index)};
            renderTarget.guest.format = [&]() -> texture::Format {
                switch (format) {
                    case maxwell3d::RenderTarget::ColorFormat::None:
                        return {};
                    case maxwell3d::RenderTarget::ColorFormat::R32B32G32A32Float:
                        return format::R32B32G32A32Float;
                    case maxwell3d::RenderTarget::ColorFormat::R16G16B16A16Unorm:
                        return format::R16G16B16A16Unorm;
                    case maxwell3d::RenderTarget::ColorFormat::R16G16B16A16Snorm:
                        return format::R16G16B16A16Snorm;
                    case maxwell3d::RenderTarget::ColorFormat::R16G16B16A16Sint:
                        return format::R16G16B16A16Sint;
                    case maxwell3d::RenderTarget::ColorFormat::R16G16B16A16Uint:
                        return format::R16G16B16A16Uint;
                    case maxwell3d::RenderTarget::ColorFormat::R16G16B16A16Float:
                        return format::R16G16B16A16Float;
                    case maxwell3d::RenderTarget::ColorFormat::A2B10G10R10Unorm:
                        return format::A2B10G10R10Unorm;
                    case maxwell3d::RenderTarget::ColorFormat::R8G8B8A8Unorm:
                        return format::R8G8B8A8Unorm;
                    case maxwell3d::RenderTarget::ColorFormat::A8B8G8R8Srgb:
                        return format::A8B8G8R8Srgb;
                    case maxwell3d::RenderTarget::ColorFormat::A8B8G8R8Snorm:
                        return format::A8B8G8R8Snorm;
                    case maxwell3d::RenderTarget::ColorFormat::R16G16Unorm:
                        return format::R16G16Unorm;
                    case maxwell3d::RenderTarget::ColorFormat::R16G16Snorm:
                        return format::R16G16Snorm;
                    case maxwell3d::RenderTarget::ColorFormat::R16G16Sint:
                        return format::R16G16Sint;
                    case maxwell3d::RenderTarget::ColorFormat::R16G16Uint:
                        return format::R16G16Uint;
                    case maxwell3d::RenderTarget::ColorFormat::R16G16Float:
                        return format::R16G16Float;
                    case maxwell3d::RenderTarget::ColorFormat::B10G11R11Float:
                        return format::B10G11R11Float;
                    case maxwell3d::RenderTarget::ColorFormat::R32Float:
                        return format::R32Float;
                    case maxwell3d::RenderTarget::ColorFormat::R8G8Unorm:
                        return format::R8G8Unorm;
                    case maxwell3d::RenderTarget::ColorFormat::R8G8Snorm:
                        return format::R8G8Snorm;
                    case maxwell3d::RenderTarget::ColorFormat::R16Unorm:
                        return format::R16Unorm;
                    case maxwell3d::RenderTarget::ColorFormat::R16Float:
                        return format::R16Float;
                    case maxwell3d::RenderTarget::ColorFormat::R8Unorm:
                        return format::R8Unorm;
                    case maxwell3d::RenderTarget::ColorFormat::R8Snorm:
                        return format::R8Snorm;
                    case maxwell3d::RenderTarget::ColorFormat::R8Sint:
                        return format::R8Sint;
                    case maxwell3d::RenderTarget::ColorFormat::R8Uint:
                        return format::R8Uint;
                    default:
                        throw exception("Cannot translate the supplied RT format: 0x{:X}", static_cast<u32>(format));
                }
            }();

            if (renderTarget.guest.tileConfig.mode == texture::TileMode::Linear && renderTarget.guest.format)
                renderTarget.guest.dimensions.width = renderTarget.widthBytes / renderTarget.guest.format->bpb;

            renderTarget.disabled = !renderTarget.guest.format;
            renderTarget.view.reset();
        }

        void SetRenderTargetTileMode(size_t index, maxwell3d::RenderTarget::TileMode mode) {
            auto &renderTarget{renderTargets.at(index)};
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

        void SetRenderTargetArrayMode(size_t index, maxwell3d::RenderTarget::ArrayMode mode) {
            auto &renderTarget{renderTargets.at(index)};
            renderTarget.guest.layerCount = mode.layerCount;
            if (mode.volume)
                throw exception("RT Array Volumes are not supported (with layer count = {})", mode.layerCount);
            renderTarget.view.reset();
        }

        void SetRenderTargetLayerStride(size_t index, u32 layerStrideLsr2) {
            auto &renderTarget{renderTargets.at(index)};
            renderTarget.guest.layerStride = layerStrideLsr2 << 2;
            renderTarget.view.reset();
        }

        void SetRenderTargetBaseLayer(size_t index, u32 baseArrayLayer) {
            auto &renderTarget{renderTargets.at(index)};
            if (baseArrayLayer > std::numeric_limits<u16>::max())
                throw exception("Base array layer ({}) exceeds the range of array count ({}) (with layer count = {})", baseArrayLayer, std::numeric_limits<u16>::max(), renderTarget.guest.layerCount);

            renderTarget.guest.baseArrayLayer = static_cast<u16>(baseArrayLayer);
            renderTarget.view.reset();
        }

        const TextureView *GetRenderTarget(size_t index) {
            auto &renderTarget{renderTargets.at(index)};
            if (renderTarget.disabled)
                return nullptr;
            else if (renderTarget.view)
                return &*renderTarget.view;

            if (renderTarget.guest.mappings.empty()) {
                auto size{std::max<u64>(renderTarget.guest.layerStride * (renderTarget.guest.layerCount - renderTarget.guest.baseArrayLayer), renderTarget.guest.format->GetSize(renderTarget.guest.dimensions))};
                auto mappings{channelCtx.asCtx->gmmu.TranslateRange(renderTarget.gpuAddress, size)};
                renderTarget.guest.mappings.assign(mappings.begin(), mappings.end());
            }

            renderTarget.guest.type = static_cast<texture::TextureType>(renderTarget.guest.dimensions.GetType());

            renderTarget.view = gpu.texture.FindOrCreate(renderTarget.guest);
            return &renderTarget.view.value();
        }

        void UpdateRenderTargetControl(maxwell3d::RenderTargetControl control) {
            renderTargetControl = control;
        }

        /* Viewport Transforms */
      private:
        std::array<vk::Viewport, maxwell3d::ViewportCount> viewports;

      public:
        /**
         * @url https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#vertexpostproc-viewport
         * @note Comments are written in the way of getting the same viewport transformations to be done on the host rather than deriving the host structure values from the guest submitted values, fundamentally the same thing but it is consistent with not assuming a certain guest API
         */
        void SetViewportX(size_t index, float scale, float translate) {
            auto &viewport{viewports.at(index)};
            viewport.x = scale - translate; // Counteract the addition of the half of the width (o_x) to the host translation
            viewport.width = scale * 2.0f; // Counteract the division of the width (p_x) by 2 for the host scale
        }

        void SetViewportY(size_t index, float scale, float translate) {
            auto &viewport{viewports.at(index)};
            viewport.y = scale - translate; // Counteract the addition of the half of the height (p_y/2 is center) to the host translation (o_y)
            viewport.height = scale * 2.0f; // Counteract the division of the height (p_y) by 2 for the host scale
        }

        void SetViewportZ(size_t index, float scale, float translate) {
            auto &viewport{viewports.at(index)};
            viewport.minDepth = translate; // minDepth (o_z) directly corresponds to the host translation
            viewport.maxDepth = scale + translate; // Counteract the subtraction of the maxDepth (p_z - o_z) by minDepth (o_z) for the host scale
        }

        /* Buffer Clears */
      private:
        vk::ClearColorValue clearColorValue{}; //!< The value written to a color buffer being cleared

      public:
        void UpdateClearColorValue(size_t index, u32 value) {
            clearColorValue.uint32.at(index) = value;
        }

        void ClearBuffers(maxwell3d::ClearBuffers clear) {
            auto renderTargetIndex{renderTargetControl[clear.renderTargetId]};
            auto renderTargetPointer{GetRenderTarget(renderTargetIndex)};
            if (renderTargetPointer) {
                auto renderTarget{*renderTargetPointer};
                std::lock_guard lock(*renderTarget.backing);

                vk::ImageAspectFlags aspect{};
                if (clear.depth)
                    aspect |= vk::ImageAspectFlagBits::eDepth;
                if (clear.stencil)
                    aspect |= vk::ImageAspectFlagBits::eStencil;
                if (clear.red || clear.green || clear.blue || clear.alpha)
                    aspect |= vk::ImageAspectFlagBits::eColor;
                aspect &= renderTarget.format->vkAspect;

                if (aspect == vk::ImageAspectFlags{})
                    return;

                auto scissor{scissors.at(renderTargetIndex)};
                scissor.extent.width = static_cast<u32>(std::min(static_cast<i32>(renderTarget.backing->dimensions.width) - scissor.offset.x,
                                                                 static_cast<i32>(scissor.extent.width)));
                scissor.extent.height = static_cast<u32>(std::min(static_cast<i32>(renderTarget.backing->dimensions.height) - scissor.offset.y,
                                                                  static_cast<i32>(scissor.extent.height)));

                if (scissor.extent.width == 0 || scissor.extent.height == 0)
                    return;

                if (scissor.extent.width == renderTarget.backing->dimensions.width && scissor.extent.height == renderTarget.backing->dimensions.height && renderTarget.range.baseArrayLayer == 0 && renderTarget.range.layerCount == 1 && clear.layerId == 0) {
                    executor.AddClearColorSubpass(renderTarget, clearColorValue);
                } else {
                    executor.AddSubpass([aspect, clearColorValue = clearColorValue, layerId = clear.layerId, scissor](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &) {
                        commandBuffer.clearAttachments(vk::ClearAttachment{
                            .aspectMask = aspect,
                            .colorAttachment = 0,
                            .clearValue = clearColorValue,
                        }, vk::ClearRect{
                            .rect = scissor,
                            .baseArrayLayer = layerId,
                            .layerCount = 1,
                        });
                    }, vk::Rect2D{
                        .extent = renderTarget.backing->dimensions,
                    }, {}, {renderTarget});
                }
            }
        }

        /* Viewport Scissors */
      private:
        std::array<vk::Rect2D, maxwell3d::ViewportCount> scissors; //!< The scissors applied to viewports/render targets for masking writes during draws or clears
        constexpr static vk::Rect2D DefaultScissor{
            .extent.height = std::numeric_limits<i32>::max(),
            .extent.width = std::numeric_limits<i32>::max(),
        }; //!< A scissor which displays the entire viewport, utilized when the viewport scissor is disabled

      public:
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

        /* Shader Program */
      private:
        struct Shader {
            bool enabled{false};
            u32 offset{}; //!< Offset of the shader from the base IOVA
            span<u8> data; //!< The shader bytecode in the CPU AS
        };

        u64 shaderBaseIova{}; //!< The base IOVA that shaders are located at an offset from
        std::array<Shader, maxwell3d::StageCount> boundShaders{};

      public:
        void SetShaderBaseIovaHigh(u32 high) {
            shaderBaseIova = (shaderBaseIova & std::numeric_limits<u32>::max()) | (static_cast<u64>(high) << 32);
            for (auto &shader : boundShaders)
                shader.data = span<u8>{};
        }

        void SetShaderBaseIovaLow(u32 low) {
            shaderBaseIova = (shaderBaseIova & (static_cast<u64>(std::numeric_limits<u32>::max()) << 32)) | low;
            for (auto &shader : boundShaders)
                shader.data = span<u8>{};
        }

        void SetShaderEnabled(maxwell3d::StageId stage, bool enabled) {
            auto &shader{boundShaders[static_cast<size_t>(stage)]};
            shader.enabled = enabled;
            shader.data = span<u8>{};
        }

        void SetShaderOffset(maxwell3d::StageId stage, u32 offset) {
            auto &shader{boundShaders[static_cast<size_t>(stage)]};
            shader.offset = offset;
            shader.data = span<u8>{};
        }
    };
}
