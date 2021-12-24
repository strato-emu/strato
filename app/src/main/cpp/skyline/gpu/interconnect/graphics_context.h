// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <boost/container/static_vector.hpp>
#include <gpu/texture/format.h>
#include <gpu/buffer_manager.h>
#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include <soc/gm20b/engines/maxwell/types.h>

#include "command_executor.h"

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
            if (!gpu.quirks.supportsMultipleViewports) {
                viewportState.viewportCount = 1;
                viewportState.scissorCount = 1;
            }

            u32 bindingIndex{};
            for (auto &vertexBuffer : vertexBuffers) {
                vertexBuffer.bindingDescription.binding = bindingIndex;
                vertexBuffer.bindingDivisorDescription.binding = bindingIndex;
                bindingIndex++;
            }
            if (!gpu.quirks.supportsVertexAttributeDivisor)
                vertexState.unlink<vk::PipelineVertexInputDivisorStateCreateInfoEXT>();

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
                using MaxwellColorRtFormat = maxwell3d::ColorRenderTarget::Format;
                switch (format) {
                    case MaxwellColorRtFormat::None:
                        return {};
                    case MaxwellColorRtFormat::R32B32G32A32Float:
                        return format::R32B32G32A32Float;
                    case MaxwellColorRtFormat::R16G16B16A16Unorm:
                        return format::R16G16B16A16Unorm;
                    case MaxwellColorRtFormat::R16G16B16A16Snorm:
                        return format::R16G16B16A16Snorm;
                    case MaxwellColorRtFormat::R16G16B16A16Sint:
                        return format::R16G16B16A16Sint;
                    case MaxwellColorRtFormat::R16G16B16A16Uint:
                        return format::R16G16B16A16Uint;
                    case MaxwellColorRtFormat::R16G16B16A16Float:
                        return format::R16G16B16A16Float;
                    case MaxwellColorRtFormat::B8G8R8A8Unorm:
                        return format::B8G8R8A8Unorm;
                    case MaxwellColorRtFormat::B8G8R8A8Srgb:
                        return format::B8G8R8A8Srgb;
                    case MaxwellColorRtFormat::A2B10G10R10Unorm:
                        return format::A2B10G10R10Unorm;
                    case MaxwellColorRtFormat::R8G8B8A8Unorm:
                        return format::R8G8B8A8Unorm;
                    case MaxwellColorRtFormat::A8B8G8R8Srgb:
                        return format::A8B8G8R8Srgb;
                    case MaxwellColorRtFormat::A8B8G8R8Snorm:
                        return format::A8B8G8R8Snorm;
                    case MaxwellColorRtFormat::R16G16Unorm:
                        return format::R16G16Unorm;
                    case MaxwellColorRtFormat::R16G16Snorm:
                        return format::R16G16Snorm;
                    case MaxwellColorRtFormat::R16G16Sint:
                        return format::R16G16Sint;
                    case MaxwellColorRtFormat::R16G16Uint:
                        return format::R16G16Uint;
                    case MaxwellColorRtFormat::R16G16Float:
                        return format::R16G16Float;
                    case MaxwellColorRtFormat::B10G11R11Float:
                        return format::B10G11R11Float;
                    case MaxwellColorRtFormat::R32Float:
                        return format::R32Float;
                    case MaxwellColorRtFormat::R8G8Unorm:
                        return format::R8G8Unorm;
                    case MaxwellColorRtFormat::R8G8Snorm:
                        return format::R8G8Snorm;
                    case MaxwellColorRtFormat::R16Unorm:
                        return format::R16Unorm;
                    case MaxwellColorRtFormat::R16Float:
                        return format::R16Float;
                    case MaxwellColorRtFormat::R8Unorm:
                        return format::R8Unorm;
                    case MaxwellColorRtFormat::R8Snorm:
                        return format::R8Snorm;
                    case MaxwellColorRtFormat::R8Sint:
                        return format::R8Sint;
                    case MaxwellColorRtFormat::R8Uint:
                        return format::R8Uint;
                    default:
                        throw exception("Cannot translate the supplied color RT format: 0x{:X}", static_cast<u32>(format));
                }
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
                auto size{std::max<u64>(renderTarget.guest.layerStride * (renderTarget.guest.layerCount - renderTarget.guest.baseArrayLayer), renderTarget.guest.format->GetSize(renderTarget.guest.dimensions))};
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
        std::array<vk::Viewport, maxwell3d::ViewportCount> viewports;
        std::array<vk::Rect2D, maxwell3d::ViewportCount> scissors; //!< The scissors applied to viewports/render targets for masking writes during draws or clears
        constexpr static vk::Rect2D DefaultScissor{
            .extent.height = std::numeric_limits<i32>::max(),
            .extent.width = std::numeric_limits<i32>::max(),
        }; //!< A scissor which displays the entire viewport, utilized when the viewport scissor is disabled
        vk::PipelineViewportStateCreateInfo viewportState{
            .pViewports = viewports.data(),
            .viewportCount = maxwell3d::ViewportCount,
            .pScissors = scissors.data(),
            .scissorCount = maxwell3d::ViewportCount,
        };

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
        }

        void SetViewportZ(size_t index, float scale, float translate) {
            auto &viewport{viewports.at(index)};
            viewport.minDepth = translate; // minDepth (o_z) directly corresponds to the host translation
            viewport.maxDepth = scale + translate; // Counteract the subtraction of the maxDepth (p_z - o_z) by minDepth (o_z) for the host scale
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
        };
        ConstantBuffer constantBufferSelector; //!< The constant buffer selector is used to bind a constant buffer to a stage or update data in it

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

        BufferView *GetConstantBufferSelectorView() {
            if (constantBufferSelector.size == 0)
                return nullptr;
            else if (constantBufferSelector.view)
                return &*constantBufferSelector.view;

            if (constantBufferSelector.guest.mappings.empty()) {
                auto mappings{channelCtx.asCtx->gmmu.TranslateRange(constantBufferSelector.iova, constantBufferSelector.size)};
                constantBufferSelector.guest.mappings.assign(mappings.begin(), mappings.end());
            }

            constantBufferSelector.view = gpu.buffer.FindOrCreate(constantBufferSelector.guest);
            return constantBufferSelector.view.get();
        }

        /* Shader Program */
      private:
        struct Shader {
            bool enabled{false};
            ShaderCompiler::Stage stage;

            bool invalidated{true}; //!< If the shader that existed earlier has been invalidated
            bool shouldCheckSame{false}; //!< If we should do a check for the shader being the same as before
            u32 offset{}; //!< Offset of the shader from the base IOVA
            std::vector<u8> data; //!< The shader bytecode in a vector
            std::optional<ShaderCompiler::IR::Program> program;

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
            ShaderSet() : array({
                                  Shader{ShaderCompiler::Stage::VertexA},
                                  Shader{ShaderCompiler::Stage::VertexB},
                                  Shader{ShaderCompiler::Stage::TessellationControl},
                                  Shader{ShaderCompiler::Stage::TessellationEval},
                                  Shader{ShaderCompiler::Stage::Geometry},
                                  Shader{ShaderCompiler::Stage::Fragment},
                              }) {}

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

            std::variant<ShaderCompiler::IR::Program, std::reference_wrapper<ShaderCompiler::IR::Program>> program; //!< The shader program by value or by reference (VertexA and VertexB shaders when combined will store by value, otherwise only a reference is stored)

            bool needsRecompile{}; //!< If the shader needs to be recompiled as runtime information has changed
            ShaderCompiler::VaryingState previousStageStores{};
            u32 bindingBase{}, bindingLast{}; //!< The base and last binding for descriptors bound to this stage
            std::optional<vk::raii::ShaderModule> vkModule;

            std::array<std::shared_ptr<BufferView>, maxwell3d::PipelineStageConstantBufferCount> constantBuffers{};

            PipelineStage(vk::ShaderStageFlagBits vkStage) : vkStage(vkStage) {}
        };

        struct PipelineStages : public std::array<PipelineStage, maxwell3d::PipelineStageCount> {
            PipelineStages() : array({
                                         PipelineStage{vk::ShaderStageFlagBits::eVertex},
                                         PipelineStage{vk::ShaderStageFlagBits::eTessellationControl},
                                         PipelineStage{vk::ShaderStageFlagBits::eTessellationEvaluation},
                                         PipelineStage{vk::ShaderStageFlagBits::eGeometry},
                                         PipelineStage{vk::ShaderStageFlagBits::eFragment},
                                     }) {}

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

        std::array<vk::PipelineShaderStageCreateInfo, maxwell3d::PipelineStageCount> shaderStagesInfo{}; //!< Storage backing for the pipeline shader stage information for all shaders aside from 'VertexA' which uses the same stage as 'VertexB'
        std::optional<vk::raii::DescriptorSetLayout> descriptorSetLayout{}; //!< The descriptor set layout for the pipeline (Only valid when `activeShaderStagesInfoCount` is non-zero)

        ShaderCompiler::RuntimeInfo runtimeInfo{};

        constexpr static size_t MaxShaderBytecodeSize{1 * 1024 * 1024}; //!< The largest shader binary that we support (1 MiB)

        constexpr static size_t PipelineUniqueDescriptorTypeCount{1}; //!< The amount of unique descriptor types that may be bound to a pipeline
        constexpr static size_t MaxPipelineDescriptorWriteCount{maxwell3d::PipelineStageCount * PipelineUniqueDescriptorTypeCount}; //!< The maxium amount of descriptors writes that are used to bind a pipeline
        constexpr static size_t MaxPipelineDescriptorCount{100}; //!< The maxium amount of descriptors we support being bound to a pipeline

        boost::container::static_vector<vk::WriteDescriptorSet, MaxPipelineDescriptorWriteCount> descriptorSetWrites;
        boost::container::static_vector<vk::DescriptorSetLayoutBinding, MaxPipelineDescriptorCount> layoutBindings;
        boost::container::static_vector<vk::DescriptorBufferInfo, MaxPipelineDescriptorCount> bufferInfo;

        /**
         * @brief All state concerning the shader programs and their bindings
         * @note The `descriptorSetWrite` will have a null `dstSet` which needs to be assigned prior to usage
         */
        struct ShaderProgramState {
            span<vk::PipelineShaderStageCreateInfo> shaders;
            vk::DescriptorSetLayout descriptorSetLayout;
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

                            shader.program = gpu.shader.ParseGraphicsShader(shader.data, shader.stage, shader.offset);

                            if (shader.stage != ShaderCompiler::Stage::VertexA && shader.stage != ShaderCompiler::Stage::VertexB) {
                                pipelineStage.program.emplace<std::reference_wrapper<ShaderCompiler::IR::Program>>(*shader.program);
                            } else if (shader.stage == ShaderCompiler::Stage::VertexA) {
                                auto &vertexB{shaders[maxwell3d::ShaderStage::VertexB]};

                                if (!vertexB.enabled)
                                    throw exception("Enabling VertexA without VertexB is not supported");
                                else if (!vertexB.invalidated)
                                    // If only VertexA is invalidated, we need to recombine here but we can defer it otherwise
                                    pipelineStage.program = gpu.shader.CombineVertexShaders(*shader.program, *vertexB.program, vertexB.data);
                            } else if (shader.stage == ShaderCompiler::Stage::VertexB) {
                                auto &vertexA{shaders[maxwell3d::ShaderStage::VertexA]};

                                if (vertexA.enabled)
                                    // We need to combine the vertex shader stages if VertexA is enabled
                                    pipelineStage.program = gpu.shader.CombineVertexShaders(*vertexA.program, *shader.program, shader.data);
                                else
                                    pipelineStage.program.emplace<std::reference_wrapper<ShaderCompiler::IR::Program>>(*shader.program);
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

            runtimeInfo.previous_stage_stores.mask.set(); // First stage should always have all bits set
            ShaderCompiler::Backend::Bindings bindings{};

            size_t count{};
            for (auto &pipelineStage : pipelineStages) {
                if (!pipelineStage.enabled)
                    continue;

                auto &program{std::visit(VariantVisitor{
                    [](ShaderCompiler::IR::Program &program) -> ShaderCompiler::IR::Program & { return program; },
                    [](std::reference_wrapper<ShaderCompiler::IR::Program> program) -> ShaderCompiler::IR::Program & { return program.get(); },
                }, pipelineStage.program)};

                if (pipelineStage.needsRecompile || bindings.unified != pipelineStage.bindingBase || pipelineStage.previousStageStores.mask != runtimeInfo.previous_stage_stores.mask) {
                    pipelineStage.previousStageStores = runtimeInfo.previous_stage_stores;
                    pipelineStage.bindingBase = bindings.unified;
                    pipelineStage.vkModule = gpu.shader.CompileShader(runtimeInfo, program, bindings);
                    pipelineStage.bindingLast = bindings.unified;
                }

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

                        auto view{pipelineStage.constantBuffers[constantBuffer.index]};
                        std::scoped_lock lock(*view);
                        bufferInfo.push_back(vk::DescriptorBufferInfo{
                            .buffer = view->buffer->GetBacking(),
                            .offset = view->offset,
                            .range = view->range,
                        });
                        executor.AttachBuffer(view.get());
                    }
                }

                shaderStagesInfo[count++] = vk::PipelineShaderStageCreateInfo{
                    .stage = pipelineStage.vkStage,
                    .module = **pipelineStage.vkModule,
                    .pName = "main",
                };
            }

            descriptorSetLayout.emplace(gpu.vkDevice, vk::DescriptorSetLayoutCreateInfo{
                .pBindings = layoutBindings.data(),
                .bindingCount = static_cast<u32>(layoutBindings.size()),
            });

            return {
                span(shaderStagesInfo.data(), count),
                **descriptorSetLayout,
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
            auto &constantBufferView{pipelineStages[stage].constantBuffers[index]};

            if (enable) {
                constantBufferView = GetConstantBufferSelectorView()->shared_from_this();
            } else {
                constantBufferView = nullptr;
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
                    return vk::BlendFactor::eOneMinusSrcColor;

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
            bool disabled{};
            vk::VertexInputBindingDescription bindingDescription{};
            vk::VertexInputBindingDivisorDescriptionEXT bindingDivisorDescription{};
            IOVA start{}, end{}; //!< IOVAs covering a contiguous region in GPU AS with the vertex buffer
            GuestBuffer guest;
            std::shared_ptr<BufferView> view;
        };
        std::array<VertexBuffer, maxwell3d::VertexBufferCount> vertexBuffers{};
        boost::container::static_vector<vk::VertexInputBindingDescription, maxwell3d::VertexBufferCount> vertexBindingDescriptions{};
        boost::container::static_vector<vk::VertexInputBindingDivisorDescriptionEXT, maxwell3d::VertexBufferCount> vertexBindingDivisorsDescriptions{};

        struct VertexAttribute {
            bool enabled{};
            vk::VertexInputAttributeDescription description;
        };
        std::array<VertexAttribute, maxwell3d::VertexAttributeCount> vertexAttributes{};
        boost::container::static_vector<vk::VertexInputAttributeDescription, maxwell3d::VertexAttributeCount> vertexAttributesDescriptions{};

        vk::StructureChain<vk::PipelineVertexInputStateCreateInfo, vk::PipelineVertexInputDivisorStateCreateInfoEXT> vertexState{
            vk::PipelineVertexInputStateCreateInfo{
                .pVertexBindingDescriptions = vertexBindingDescriptions.data(),
                .pVertexAttributeDescriptions = vertexAttributesDescriptions.data(),
            }, vk::PipelineVertexInputDivisorStateCreateInfoEXT{
                .pVertexBindingDivisors = vertexBindingDivisorsDescriptions.data(),
            }
        };

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

            switch (size | type) {
                // @fmt:off

                /* 8-bit components */

                case Size::e1x8 | Type::Unorm: return vk::Format::eR8Unorm;
                case Size::e1x8 | Type::Snorm: return vk::Format::eR8Snorm;
                case Size::e1x8 | Type::Uint: return vk::Format::eR8Uint;
                case Size::e1x8 | Type::Sint: return vk::Format::eR8Sint;
                case Size::e1x8 | Type::Uscaled: return vk::Format::eR8Uscaled;
                case Size::e1x8 | Type::Sscaled: return vk::Format::eR8Sscaled;

                case Size::e2x8 | Type::Unorm: return vk::Format::eR8G8Unorm;
                case Size::e2x8 | Type::Snorm: return vk::Format::eR8G8Snorm;
                case Size::e2x8 | Type::Uint: return vk::Format::eR8G8Uint;
                case Size::e2x8 | Type::Sint: return vk::Format::eR8G8Sint;
                case Size::e2x8 | Type::Uscaled: return vk::Format::eR8G8Uscaled;
                case Size::e2x8 | Type::Sscaled: return vk::Format::eR8G8Sscaled;

                case Size::e3x8 | Type::Unorm: return vk::Format::eR8G8B8Unorm;
                case Size::e3x8 | Type::Snorm: return vk::Format::eR8G8B8Snorm;
                case Size::e3x8 | Type::Uint: return vk::Format::eR8G8B8Uint;
                case Size::e3x8 | Type::Sint: return vk::Format::eR8G8B8Sint;
                case Size::e3x8 | Type::Uscaled: return vk::Format::eR8G8B8Uscaled;
                case Size::e3x8 | Type::Sscaled: return vk::Format::eR8G8B8Sscaled;

                case Size::e4x8 | Type::Unorm: return vk::Format::eR8G8B8A8Unorm;
                case Size::e4x8 | Type::Snorm: return vk::Format::eR8G8B8A8Snorm;
                case Size::e4x8 | Type::Uint: return vk::Format::eR8G8B8A8Uint;
                case Size::e4x8 | Type::Sint: return vk::Format::eR8G8B8A8Sint;
                case Size::e4x8 | Type::Uscaled: return vk::Format::eR8G8B8A8Uscaled;
                case Size::e4x8 | Type::Sscaled: return vk::Format::eR8G8B8A8Sscaled;

                /* 16-bit components */

                case Size::e1x16 | Type::Unorm: return vk::Format::eR16Unorm;
                case Size::e1x16 | Type::Snorm: return vk::Format::eR16Snorm;
                case Size::e1x16 | Type::Uint: return vk::Format::eR16Uint;
                case Size::e1x16 | Type::Sint: return vk::Format::eR16Sint;
                case Size::e1x16 | Type::Uscaled: return vk::Format::eR16Uscaled;
                case Size::e1x16 | Type::Sscaled: return vk::Format::eR16Sscaled;
                case Size::e1x16 | Type::Float: return vk::Format::eR16Sfloat;

                case Size::e2x16 | Type::Unorm: return vk::Format::eR16G16Unorm;
                case Size::e2x16 | Type::Snorm: return vk::Format::eR16G16Snorm;
                case Size::e2x16 | Type::Uint: return vk::Format::eR16G16Uint;
                case Size::e2x16 | Type::Sint: return vk::Format::eR16G16Sint;
                case Size::e2x16 | Type::Uscaled: return vk::Format::eR16G16Uscaled;
                case Size::e2x16 | Type::Sscaled: return vk::Format::eR16G16Sscaled;
                case Size::e2x16 | Type::Float: return vk::Format::eR16G16Sfloat;

                case Size::e3x16 | Type::Unorm: return vk::Format::eR16G16B16Unorm;
                case Size::e3x16 | Type::Snorm: return vk::Format::eR16G16B16Snorm;
                case Size::e3x16 | Type::Uint: return vk::Format::eR16G16B16Uint;
                case Size::e3x16 | Type::Sint: return vk::Format::eR16G16B16Sint;
                case Size::e3x16 | Type::Uscaled: return vk::Format::eR16G16B16Uscaled;
                case Size::e3x16 | Type::Sscaled: return vk::Format::eR16G16B16Sscaled;
                case Size::e3x16 | Type::Float: return vk::Format::eR16G16B16Sfloat;

                case Size::e4x16 | Type::Unorm: return vk::Format::eR16G16B16A16Unorm;
                case Size::e4x16 | Type::Snorm: return vk::Format::eR16G16B16A16Snorm;
                case Size::e4x16 | Type::Uint: return vk::Format::eR16G16B16A16Uint;
                case Size::e4x16 | Type::Sint: return vk::Format::eR16G16B16A16Sint;
                case Size::e4x16 | Type::Uscaled: return vk::Format::eR16G16B16A16Uscaled;
                case Size::e4x16 | Type::Sscaled: return vk::Format::eR16G16B16A16Sscaled;
                case Size::e4x16 | Type::Float: return vk::Format::eR16G16B16A16Sfloat;

                /* 32-bit components */

                case Size::e1x32 | Type::Uint: return vk::Format::eR32Uint;
                case Size::e1x32 | Type::Sint: return vk::Format::eR32Sint;
                case Size::e1x32 | Type::Float: return vk::Format::eR32Sfloat;

                case Size::e2x32 | Type::Uint: return vk::Format::eR32G32Uint;
                case Size::e2x32 | Type::Sint: return vk::Format::eR32G32Sint;
                case Size::e2x32 | Type::Float: return vk::Format::eR32G32Sfloat;

                case Size::e3x32 | Type::Uint: return vk::Format::eR32G32B32Uint;
                case Size::e3x32 | Type::Sint: return vk::Format::eR32G32B32Sint;
                case Size::e3x32 | Type::Float: return vk::Format::eR32G32B32Sfloat;

                case Size::e4x32 | Type::Uint: return vk::Format::eR32G32B32A32Uint;
                case Size::e4x32 | Type::Sint: return vk::Format::eR32G32B32A32Sint;
                case Size::e4x32 | Type::Float: return vk::Format::eR32G32B32A32Sfloat;

                /* 10-bit RGB, 2-bit A */

                case Size::e10_10_10_2 | Type::Unorm: return vk::Format::eA2R10G10B10UnormPack32;
                case Size::e10_10_10_2 | Type::Snorm: return vk::Format::eA2R10G10B10SnormPack32;
                case Size::e10_10_10_2 | Type::Uint: return vk::Format::eA2R10G10B10UintPack32;
                case Size::e10_10_10_2 | Type::Sint: return vk::Format::eA2R10G10B10SintPack32;
                case Size::e10_10_10_2 | Type::Uscaled: return vk::Format::eA2R10G10B10UscaledPack32;
                case Size::e10_10_10_2 | Type::Sscaled: return vk::Format::eA2R10G10B10SscaledPack32;

                /* Unknown */

                case 0x12F: return vk::Format::eUndefined; // Issued by Maxwell3D::InitializeRegisters()

                // @fmt:on

                default:
                    throw exception("Unimplemented Maxwell3D Vertex Buffer Format: {} | {}", maxwell3d::VertexAttribute::ToString(size), maxwell3d::VertexAttribute::ToString(type));
            }
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
            if (vertexBuffer.disabled || vertexBuffer.start > vertexBuffer.end || vertexBuffer.start == 0 || vertexBuffer.end == 0)
                return nullptr;
            else if (vertexBuffer.view)
                return &*vertexBuffer.view;

            if (vertexBuffer.guest.mappings.empty()) {
                auto mappings{channelCtx.asCtx->gmmu.TranslateRange(vertexBuffer.start, (vertexBuffer.end + 1) - vertexBuffer.start)};
                vertexBuffer.guest.mappings.assign(mappings.begin(), mappings.end());
            }

            vertexBuffer.view = gpu.buffer.FindOrCreate(vertexBuffer.guest);
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

        /* Depth */
        vk::PipelineDepthStencilStateCreateInfo depthState{};

        /* Multisampling */
        vk::PipelineMultisampleStateCreateInfo multisampleState{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
        };

        /* Draws */
      private:
        vk::GraphicsPipelineCreateInfo pipelineState{
            .pVertexInputState = &vertexState.get<vk::PipelineVertexInputStateCreateInfo>(),
            .pInputAssemblyState = &inputAssemblyState,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizerState.get<vk::PipelineRasterizationStateCreateInfo>(),
            .pMultisampleState = &multisampleState,
            .pDepthStencilState = &depthState,
            .pColorBlendState = &blendState,
            .pDynamicState = nullptr,
        };
        vk::raii::PipelineCache pipelineCache;

      public:
        void Draw(u32 vertexCount, u32 firstVertex) {
            // Color Render Target Setup
            boost::container::static_vector<std::scoped_lock<TextureView>, maxwell3d::RenderTargetCount> colorRenderTargetLocks;
            boost::container::static_vector<TextureView *, maxwell3d::RenderTargetCount> activeColorRenderTargets;

            for (u32 index{}; index < maxwell3d::RenderTargetCount; index++) {
                auto renderTarget{GetColorRenderTarget(index)};
                if (renderTarget) {
                    colorRenderTargetLocks.emplace_back(*renderTarget);
                    activeColorRenderTargets.push_back(renderTarget);
                }
            }

            blendState.attachmentCount = static_cast<u32>(activeColorRenderTargets.size());

            // Depth/Stencil Render Target Setup
            auto depthRenderTargetView{GetDepthRenderTarget()};
            std::optional<std::scoped_lock<TextureView>> depthTargetLock;
            if (depthRenderTargetView)
                depthTargetLock.emplace(*depthRenderTargetView);

            // Vertex Buffer Setup
            boost::container::static_vector<std::scoped_lock<BufferView>, maxwell3d::VertexBufferCount> vertexBufferLocks;
            std::array<vk::Buffer, maxwell3d::VertexBufferCount> vertexBufferHandles{};
            std::array<vk::DeviceSize, maxwell3d::VertexBufferCount> vertexBufferOffsets{};

            vertexBindingDescriptions.clear();
            vertexBindingDivisorsDescriptions.clear();

            for (u32 index{}; index < maxwell3d::VertexBufferCount; index++) {
                auto vertexBufferView{GetVertexBuffer(index)};
                if (vertexBufferView) {
                    auto &vertexBuffer{vertexBuffers[index]};
                    vertexBindingDescriptions.push_back(vertexBuffer.bindingDescription);
                    vertexBindingDivisorsDescriptions.push_back(vertexBuffer.bindingDivisorDescription);

                    vertexBufferLocks.emplace_back(*vertexBufferView);
                    executor.AttachBuffer(vertexBufferView);
                    vertexBufferHandles[index] = vertexBufferView->buffer->GetBacking();
                    vertexBufferOffsets[index] = vertexBufferView->offset;
                }
            }

            vertexState.get<vk::PipelineVertexInputStateCreateInfo>().vertexBindingDescriptionCount = static_cast<u32>(vertexBindingDescriptions.size());
            vertexState.get<vk::PipelineVertexInputDivisorStateCreateInfoEXT>().vertexBindingDivisorCount = static_cast<u32>(vertexBindingDivisorsDescriptions.size());

            // Vertex Attribute Setup
            vertexAttributesDescriptions.clear();

            for (auto &vertexAttribute : vertexAttributes)
                if (vertexAttribute.enabled)
                    vertexAttributesDescriptions.push_back(vertexAttribute.description);

            vertexState.get<vk::PipelineVertexInputStateCreateInfo>().vertexAttributeDescriptionCount = static_cast<u32>(vertexAttributesDescriptions.size());

            // Shader + Binding Setup
            auto programState{CompileShaderProgramState()};
            pipelineState.pStages = programState.shaders.data();
            pipelineState.stageCount = static_cast<u32>(programState.shaders.size());

            auto descriptorSet{gpu.descriptor.AllocateSet(programState.descriptorSetLayout)};
            for (auto &descriptorSetWrite : programState.descriptorSetWrites)
                descriptorSetWrite.dstSet = descriptorSet;
            gpu.vkDevice.updateDescriptorSets(programState.descriptorSetWrites, nullptr);

            vk::raii::PipelineLayout pipelineLayout(gpu.vkDevice, vk::PipelineLayoutCreateInfo{
                .pSetLayouts = &programState.descriptorSetLayout,
                .setLayoutCount = 1,
            });

            // Draw Persistent Storage
            struct Storage : FenceCycleDependency {
                vk::raii::PipelineLayout pipelineLayout;
                std::optional<vk::raii::Pipeline> pipeline;
                DescriptorAllocator::ActiveDescriptorSet descriptorSet;

                Storage(vk::raii::PipelineLayout &&pipelineLayout, DescriptorAllocator::ActiveDescriptorSet &&descriptorSet) : pipelineLayout(std::move(pipelineLayout)), descriptorSet(std::move(descriptorSet)) {}
            };

            auto storage{std::make_shared<Storage>(std::move(pipelineLayout), std::move(descriptorSet))};

            // Submit Draw
            executor.AddSubpass([vertexCount, firstVertex, &vkDevice = gpu.vkDevice, pipelineCreateInfo = pipelineState, storage = std::move(storage), vertexBufferHandles = std::move(vertexBufferHandles), vertexBufferOffsets = std::move(vertexBufferOffsets), pipelineCache = *pipelineCache](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle, GPU &, vk::RenderPass renderPass, u32 subpassIndex) mutable {
                pipelineCreateInfo.layout = *storage->pipelineLayout;

                pipelineCreateInfo.renderPass = renderPass;
                pipelineCreateInfo.subpass = subpassIndex;

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

                commandBuffer.draw(vertexCount, 1, firstVertex, 0);

                storage->pipeline = vk::raii::Pipeline(vkDevice, pipeline.value);

                cycle->AttachObject(storage);
            }, vk::Rect2D{
                .extent = activeColorRenderTargets[0]->texture->dimensions,
            }, {}, activeColorRenderTargets, depthRenderTargetView);
        }
    };
}
