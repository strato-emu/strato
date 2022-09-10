// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"
#include <tuple>

namespace skyline::gpu::interconnect::maxwell3d {
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wbitfield-enum-conversion"

    /**
     * @brief Packed struct of pipeline state suitable for use as a map key
     * @note This is heavily based around yuzu's pipeline key with some packing modifications
     * @url https://github.com/yuzu-emu/yuzu/blob/9c701774562ea490296b9cbea3dbd8c096bc4483/src/video_core/renderer_vulkan/fixed_pipeline_state.h#L20
     */
    struct PackedPipelineState {
        std::array<u64, engine::PipelineCount> shaderHashes;

        struct StencilOps {
            vk::StencilOp zPass : 3;
            vk::StencilOp fail : 3;
            vk::StencilOp zFail : 3;
            vk::CompareOp func : 3;
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
            vk::PolygonMode polygonMode : 2; //!< Use SetPolygonMode
            VkCullModeFlags cullMode : 2; //!< Use SetCullMode
            bool flipYEnable : 1;
            bool frontFaceClockwise : 1; //!< With Y flip transformation already applied
            bool depthBiasEnable : 1;
            engine::ProvokingVertex::Value provokingVertex : 1;
            bool depthTestEnable : 1;
            bool depthWriteEnable : 1;
            vk::CompareOp depthFunc : 3; //!< Use SetDepthFunc
            bool depthBoundsTestEnable : 1;
            bool stencilTestEnable : 1;
            bool logicOpEnable : 1;
            vk::LogicOp logicOp : 4; //!< Use SetLogicOp
            u8 bindlessTextureConstantBufferSlotSelect : 5;
            bool apiMandatedEarlyZ : 1;
            bool openGlNdc : 1;
        };

        u32 patchSize;
        float pointSize;
        std::array<engine::VertexAttribute, engine::VertexAttributeCount> vertexAttributes;
        std::array<u8, engine::ColorTargetCount> colorRenderTargetFormats; //!< Use {Set, Get}ColorRenderTargetFormat
        std::array<u32, 8> postVtgShaderAttributeSkipMask;

        struct VertexBinding {
            u16 stride : 12;
            vk::VertexInputRate inputRate : 1;
            bool enable : 1;
            u8 _pad_ : 2;
            u32 divisor;
        };

        std::array<VertexBinding, engine::VertexStreamCount> vertexBindings; //!< Use {Set, Get}VertexBinding

        struct AttachmentBlendState {
            VkColorComponentFlags colorWriteMask : 4;
            vk::BlendOp colorBlendOp : 3;
            vk::BlendFactor srcColorBlendFactor : 5;
            vk::BlendFactor dstColorBlendFactor : 5;
            vk::BlendOp alphaBlendOp : 3;
            vk::BlendFactor srcAlphaBlendFactor : 5;
            vk::BlendFactor dstAlphaBlendFactor : 5;
            bool blendEnable : 1;
        };

        std::array<AttachmentBlendState, engine::ColorTargetCount> attachmentBlendStates;

        void SetColorRenderTargetFormat(size_t index, engine::ColorTarget::Format format);

        void SetDepthRenderTargetFormat(engine::ZtFormat format);

        void SetVertexBinding(u32 index, engine::VertexStream stream, engine::VertexStreamInstance instance);

        void SetTessellationParameters(engine::TessellationParameters parameters);

        void SetPolygonMode(engine::PolygonMode mode);

        void SetCullMode(bool enable, engine::CullFace mode);

        void SetDepthFunc(engine::CompareFunc func);

        void SetStencilOps(engine::StencilOps front, engine::StencilOps back);

        void SetLogicOp(engine::LogicOp::Func op);

        void SetAttachmentBlendState(u32 index, bool enable, engine::CtWrite writeMask, engine::Blend blend);

        void SetAttachmentBlendState(u32 index, bool enable, engine::CtWrite writeMask, engine::BlendPerTarget blend);

        std::array<vk::StencilOpState, 2> GetStencilOpsState() const;

        vk::PipelineColorBlendAttachmentState GetAttachmentBlendState(u32 index) const;

        bool operator==(const PackedPipelineState &other) const {
            return std::memcmp(this, &other, sizeof(PackedPipelineState)) == 0;
        }
    };

    #pragma clang diagnostic pop
}
