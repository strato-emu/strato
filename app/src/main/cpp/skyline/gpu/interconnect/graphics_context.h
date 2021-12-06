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
        GraphicsContext(GPU &gpu, soc::gm20b::ChannelContext &channelCtx, gpu::interconnect::CommandExecutor &executor) : gpu(gpu), channelCtx(channelCtx), executor(executor) {
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

        std::array<RenderTarget, maxwell3d::RenderTargetCount> renderTargets{}; //!< The target textures to render into as color attachments
        maxwell3d::RenderTargetControl renderTargetControl{};

      public:
        void SetRenderTargetAddressHigh(size_t index, u32 high) {
            auto &renderTarget{renderTargets.at(index)};
            renderTarget.iova.high = high;
            renderTarget.guest.mappings.clear();
            renderTarget.view.reset();
        }

        void SetRenderTargetAddressLow(size_t index, u32 low) {
            auto &renderTarget{renderTargets.at(index)};
            renderTarget.iova.low = low;
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
                    case maxwell3d::RenderTarget::ColorFormat::B8G8R8A8Unorm:
                        return format::B8G8R8A8Unorm;
                    case maxwell3d::RenderTarget::ColorFormat::B8G8R8A8Srgb:
                        return format::B8G8R8A8Srgb;
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

        TextureView *GetRenderTarget(size_t index) {
            auto &renderTarget{renderTargets.at(index)};
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
        vk::ClearColorValue clearColorValue{}; //!< The value written to a color buffer being cleared

      public:
        void UpdateClearColorValue(size_t index, u32 value) {
            clearColorValue.uint32.at(index) = value;
        }

        void ClearBuffers(maxwell3d::ClearBuffers clear) {
            auto renderTargetIndex{renderTargetControl[clear.renderTargetId]};
            auto renderTarget{GetRenderTarget(renderTargetIndex)};
            if (renderTarget) {
                std::lock_guard lock(*renderTarget->texture);

                vk::ImageAspectFlags aspect{};
                if (clear.depth)
                    aspect |= vk::ImageAspectFlagBits::eDepth;
                if (clear.stencil)
                    aspect |= vk::ImageAspectFlagBits::eStencil;
                if (clear.red || clear.green || clear.blue || clear.alpha)
                    aspect |= vk::ImageAspectFlagBits::eColor;
                aspect &= renderTarget->format->vkAspect;

                if (aspect == vk::ImageAspectFlags{})
                    return;

                auto scissor{scissors.at(renderTargetIndex)};
                scissor.extent.width = static_cast<u32>(std::min(static_cast<i32>(renderTarget->texture->dimensions.width) - scissor.offset.x,
                                                                 static_cast<i32>(scissor.extent.width)));
                scissor.extent.height = static_cast<u32>(std::min(static_cast<i32>(renderTarget->texture->dimensions.height) - scissor.offset.y,
                                                                  static_cast<i32>(scissor.extent.height)));

                if (scissor.extent.width == 0 || scissor.extent.height == 0)
                    return;

                if (scissor.extent.width == renderTarget->texture->dimensions.width && scissor.extent.height == renderTarget->texture->dimensions.height && renderTarget->range.baseArrayLayer == 0 && renderTarget->range.layerCount == 1 && clear.layerId == 0) {
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
                        .extent = renderTarget->texture->dimensions,
                    }, {}, {renderTarget});
                }
            }
        }

        /* Shader Program */
      private:
        struct Shader {
            bool enabled{false};
            u32 offset{}; //!< Offset of the shader from the base IOVA
            span<u8> data; //!< The shader bytecode in the CPU AS
        };

        IOVA shaderBaseIova{}; //!< The base IOVA that shaders are located at an offset from
        std::array<Shader, maxwell3d::StageCount> boundShaders{};

      public:
        void SetShaderBaseIovaHigh(u32 high) {
            shaderBaseIova.high = high;
            for (auto &shader : boundShaders)
                shader.data = span<u8>{};
        }

        void SetShaderBaseIovaLow(u32 low) {
            shaderBaseIova.low = low;
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
            } else {
                vertexAttribute.enabled = false;
            }
        }

        /* Input Assembly */
      private:
        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState{};

      public:
        void SetPrimitiveTopology(maxwell3d::PrimitiveTopology topology) {
            inputAssemblyState.topology = [topology]() {
                switch (topology) {
                    // @fmt:off

                    case maxwell3d::PrimitiveTopology::PointList: return vk::PrimitiveTopology::ePointList;

                    case maxwell3d::PrimitiveTopology::LineList: return vk::PrimitiveTopology::eLineList;
                    case maxwell3d::PrimitiveTopology::LineListWithAdjacency: return vk::PrimitiveTopology::eLineListWithAdjacency;
                    case maxwell3d::PrimitiveTopology::LineStrip: return vk::PrimitiveTopology::eLineStrip;
                    case maxwell3d::PrimitiveTopology::LineStripWithAdjacency: return vk::PrimitiveTopology::eLineStripWithAdjacency;

                    case maxwell3d::PrimitiveTopology::TriangleList: return vk::PrimitiveTopology::eTriangleList;
                    case maxwell3d::PrimitiveTopology::TriangleListWithAdjacency: return vk::PrimitiveTopology::eTriangleListWithAdjacency;
                    case maxwell3d::PrimitiveTopology::TriangleStrip: return vk::PrimitiveTopology::eTriangleStrip;
                    case maxwell3d::PrimitiveTopology::TriangleStripWithAdjacency: return vk::PrimitiveTopology::eTriangleStripWithAdjacency;
                    case maxwell3d::PrimitiveTopology::TriangleFan: return vk::PrimitiveTopology::eTriangleFan;

                    case maxwell3d::PrimitiveTopology::PatchList: return vk::PrimitiveTopology::ePatchList;

                    // @fmt:on

                    default:
                        throw exception("Unimplemented Maxwell3D Primitive Topology: {}", maxwell3d::ToString(topology));
                }
            }();
        }
    };
}
