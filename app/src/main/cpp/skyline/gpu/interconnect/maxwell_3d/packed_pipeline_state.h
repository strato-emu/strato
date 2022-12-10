// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <tuple>
#include <gpu/texture/format.h>
#include <shader_compiler/runtime_info.h>
#include "common.h"

namespace skyline::gpu::interconnect::maxwell3d {
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wbitfield-enum-conversion"

    /**
     * @brief Packed struct of pipeline state suitable for use as a map key
     * @note This is heavily based around yuzu's pipeline key with some packing modifications
     * @note Any modifications to this struct *MUST* be accompanied by a pipeline cache version bump
     * @url https://github.com/yuzu-emu/yuzu/blob/9c701774562ea490296b9cbea3dbd8c096bc4483/src/video_core/renderer_vulkan/fixed_pipeline_state.h#L20
     */
    struct PackedPipelineState {
        std::array<u64, engine::PipelineCount> shaderHashes;

        struct StencilOps {
            u8 zPass : 3;
            u8 fail : 3;
            u8 zFail : 3;
            u8 func : 3;
            // 4 bits left for each stencil side
        };

        StencilOps stencilFront; //!< Use {Set, Get}StencilOps
        StencilOps stencilBack; //!< Use {Set, Get}StencilOps

        struct {
            u8 depthRenderTargetFormat : 5; //!< Use {Set, Get}DepthRenderTargetFormat
            engine::DrawTopology topology : 4;
            bool primitiveRestartEnabled : 1;
            engine::TessellationParameters::DomainType domainType : 2;  //!< Use SetTessellationParameters
            engine::TessellationParameters::Spacing spacing : 2; //!< Use SetTessellationParameters
            engine::TessellationParameters::OutputPrimitives outputPrimitives : 2; //!< Use SetTessellationParameters
            bool rasterizerDiscardEnable : 1;
            u8 polygonMode : 2; //!< Use {Set,Get}PolygonMode
            VkCullModeFlags cullMode : 2; //!< Use SetCullMode
            bool flipYEnable : 1;
            bool frontFaceClockwise : 1; //!< With Y flip transformation already applied
            bool depthBiasEnable : 1;
            engine::ProvokingVertex::Value provokingVertex : 1;
            bool depthTestEnable : 1;
            bool depthWriteEnable : 1;
            u8 depthFunc : 3; //!< Use {Set,Get}DepthFunc
            bool depthBoundsTestEnable : 1;
            bool stencilTestEnable : 1;
            bool logicOpEnable : 1;
            u8 logicOp : 4; //!< Use {Set,Get}LogicOp
            u8 bindlessTextureConstantBufferSlotSelect : 5;
            bool apiMandatedEarlyZ : 1;
            bool openGlNdc : 1;
            bool transformFeedbackEnable : 1;
            u8 alphaFunc : 3; //!< Use {Set,Get}AlphaFunc
            bool alphaTestEnable : 1;
            bool depthClampEnable : 1; // Use SetDepthClampEnable
            bool dynamicStateActive : 1;
            bool viewportTransformEnable : 1;
        };

        u32 patchSize;
        float alphaRef;
        float pointSize;
        std::array<engine::VertexAttribute, engine::VertexAttributeCount> vertexAttributes;
        std::array<u8, engine::ColorTargetCount> colorRenderTargetFormats; //!< Use {Set, Get}ColorRenderTargetFormat
        engine::CtSelect ctSelect;
        std::array<u32, 8> postVtgShaderAttributeSkipMask;

        struct VertexBinding {
            u8 inputRate : 1;
            bool enable : 1;
            u32 divisor;

            vk::VertexInputRate GetInputRate() const {
                return static_cast<vk::VertexInputRate>(inputRate);
            }
        };

        std::array<VertexBinding, engine::VertexStreamCount> vertexBindings; //!< Use {Set, Get}VertexBinding

        struct AttachmentBlendState {
            VkColorComponentFlags colorWriteMask : 4;
            u8 colorBlendOp : 3;
            u8 srcColorBlendFactor : 5;
            u8 dstColorBlendFactor : 5;
            u8 alphaBlendOp : 3;
            u8 srcAlphaBlendFactor : 5;
            u8 dstAlphaBlendFactor : 5;
            bool blendEnable : 1;
        };

        std::array<AttachmentBlendState, engine::ColorTargetCount> attachmentBlendStates;

        std::array<u16, engine::VertexStreamCount> vertexStrides; //!< Use {Set, Get}VertexBinding

        struct TransformFeedbackVarying {
            u16 stride;
            u8 offsetWords;
            u8 buffer : 7;
            bool valid : 1;
        };
        std::array<TransformFeedbackVarying, 0x100> transformFeedbackVaryings{};

        /**
         * @param rawIndex Index in HW ignoring the ctSelect register
         */
        void SetColorRenderTargetFormat(size_t rawIndex, engine::ColorTarget::Format format);

        void SetDepthRenderTargetFormat(engine::ZtFormat format, bool enabled);

        void SetVertexBinding(u32 index, engine::VertexStream stream, engine::VertexStreamInstance instance);

        void SetTessellationParameters(engine::TessellationParameters parameters);

        void SetPolygonMode(engine::PolygonMode mode);

        vk::PolygonMode GetPolygonMode() const;

        void SetCullMode(bool enable, engine::CullFace mode);

        void SetDepthFunc(engine::CompareFunc func);

        vk::CompareOp GetDepthFunc() const;

        void SetStencilOps(engine::StencilOps front, engine::StencilOps back);

        void SetLogicOp(engine::LogicOp::Func op);

        vk::LogicOp GetLogicOp() const;

        void SetAttachmentBlendState(u32 index, bool enable, engine::CtWrite writeMask, engine::Blend blend);

        void SetAttachmentBlendState(u32 index, bool enable, engine::CtWrite writeMask, engine::BlendPerTarget blend);

        /**
         * @param rawIndex Index in HW ignoring the ctSelect register
         */
        texture::Format GetColorRenderTargetFormat(size_t rawIndex) const;

        /**
         * @param rawIndex Index in HW ignoring the ctSelect register
         */
        bool IsColorRenderTargetEnabled(size_t rawIndex) const;

        size_t GetColorRenderTargetCount() const;

        texture::Format GetDepthRenderTargetFormat() const;

        std::array<vk::StencilOpState, 2> GetStencilOpsState() const;

        vk::PipelineColorBlendAttachmentState GetAttachmentBlendState(u32 index) const;

        void SetTransformFeedbackVaryings(const engine::StreamOutControl &control, const std::array<u8, engine::StreamOutLayoutSelectAttributeCount> &layoutSelect, size_t buffer);

        std::vector<Shader::TransformFeedbackVarying> GetTransformFeedbackVaryings() const;

        void SetAlphaFunc(engine::CompareFunc func);

        Shader::CompareFunction GetAlphaFunc() const;

        void SetDepthClampEnable(engine::ViewportClipControl::GeometryClip clip);

        bool operator==(const PackedPipelineState &other) const {
            // Only hash transform feedback state if it's enabled
            if (other.transformFeedbackEnable && transformFeedbackEnable)
                return std::memcmp(this, &other, sizeof(PackedPipelineState)) == 0;
            else if (dynamicStateActive)
                return std::memcmp(this, &other, offsetof(PackedPipelineState, vertexStrides)) == 0;
            else
                return std::memcmp(this, &other, offsetof(PackedPipelineState, transformFeedbackVaryings)) == 0;
        }
    };

    struct PackedPipelineStateHash {
        size_t operator()(const PackedPipelineState &state) const noexcept {
            // Only hash transform feedback state if it's enabled
            if (state.transformFeedbackEnable)
                return XXH64(&state, sizeof(PackedPipelineState), 0);
            else if (state.dynamicStateActive)
                return XXH64(&state, offsetof(PackedPipelineState, vertexStrides), 0);


            return XXH64(&state, offsetof(PackedPipelineState, transformFeedbackVaryings), 0);
        }
    };

    #pragma clang diagnostic pop
}
