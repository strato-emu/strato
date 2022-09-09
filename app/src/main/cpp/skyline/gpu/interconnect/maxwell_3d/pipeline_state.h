// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <boost/container/static_vector.hpp>
#include <gpu/texture/texture.h>
#include "common.h"
#include "shader_state.h"

namespace skyline::gpu::interconnect::maxwell3d {
    /**
     * @brief Packed struct of pipeline state suitable for use as a map key
     * @note This is heavily based around yuzu's pipeline key with some packing modifications
     * @url https://github.com/yuzu-emu/yuzu/blob/9c701774562ea490296b9cbea3dbd8c096bc4483/src/video_core/renderer_vulkan/fixed_pipeline_state.h#L20
     */
    struct PackedPipelineState {
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
            vk::PolygonMode polygonMode : 2; //!< Use {Set, Get}PolygonMode
            VkCullModeFlags cullMode : 2; //!< Use {Set, Get}CullMode
            bool flipYEnable : 1;
            bool frontFaceClockwise : 1; //!< With Y flip transformation already applied
            bool depthBiasEnable : 1;
            engine::ProvokingVertex::Value provokingVertex : 1;
            bool depthTestEnable : 1;
            bool depthWriteEnable : 1;
            vk::CompareOp depthFunc : 3; //!< Use {Set, Get}DepthFunc
            bool depthBoundsTestEnable : 1;
            bool stencilTestEnable : 1;
            bool logicOpEnable : 1;
            vk::LogicOp logicOp : 4; //!< Use {Set, Get}LogicOp
            u8 bindlessTextureConstantBufferSlotSelect : 5;
        };

        u32 patchSize;
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
    };

    class ColorRenderTargetState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            const engine::ColorTarget &colorTarget;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;
        size_t index;

      public:
        ColorRenderTargetState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine, size_t index);

        std::shared_ptr<TextureView> view;

        void Flush(InterconnectContext &ctx, PackedPipelineState &packedState);
    };

    class DepthRenderTargetState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            const engine::ZtSize &ztSize;
            const soc::gm20b::engine::Address &ztOffset;
            const engine::ZtFormat &ztFormat;
            const engine::ZtBlockSize &ztBlockSize;
            const u32 &ztArrayPitch;
            const engine::ZtSelect &ztSelect;
            const engine::ZtLayer &ztLayer;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

      public:
        DepthRenderTargetState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        std::shared_ptr<TextureView> view;

        void Flush(InterconnectContext &ctx, PackedPipelineState &packedState);
    };

    class VertexInputState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            const std::array<engine::VertexStream, engine::VertexStreamCount> &vertexStreams;
            const std::array<engine::VertexStreamInstance, engine::VertexStreamCount> &vertexStreamInstance;
            const std::array<engine::VertexAttribute, engine::VertexAttributeCount> &vertexAttributes;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

      public:
        VertexInputState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(PackedPipelineState &packedState);
    };

    class InputAssemblyState {
      public:
        struct EngineRegisters {
            const u32 &primitiveRestartEnable;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        EngineRegisters engine;
        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState{};
        engine::DrawTopology currentEngineTopology{};

      public:
        InputAssemblyState(const EngineRegisters &engine);

        void Update(PackedPipelineState &packedState);

        void SetPrimitiveTopology(engine::DrawTopology topology);

        engine::DrawTopology GetPrimitiveTopology() const;

        bool NeedsQuadConversion() const;
    };

    class TessellationState {
      public:
        struct EngineRegisters {
            const u32 &patchSize;
            const engine::TessellationParameters &tessellationParameters;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        EngineRegisters engine;

      public:
        TessellationState(const EngineRegisters &engine);

        void Update(PackedPipelineState &packedState);
    };

    /**
     * @brief Holds pipeline state that is directly written by the engine code, without using dirty tracking
     */
    struct DirectPipelineState {
        InputAssemblyState inputAssembly;
    };

    class RasterizationState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            const u32 &rasterEnable;
            const engine::PolygonMode &frontPolygonMode;
            const engine::PolygonMode &backPolygonMode;
            const u32 &oglCullEnable;
            const engine::CullFace &oglCullFace;
            const engine::WindowOrigin &windowOrigin;
            const engine::FrontFace &oglFrontFace;
            const engine::ViewportClipControl &viewportClipControl;
            const engine::PolyOffset &polyOffset;
            const engine::ProvokingVertex &provokingVertex;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

      public:
        vk::StructureChain<vk::PipelineRasterizationStateCreateInfo, vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT> rasterizationState{};

        RasterizationState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(PackedPipelineState &packedState);
    };

    class DepthStencilState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            const u32 &depthTestEnable;
            const u32 &depthWriteEnable;
            const engine::CompareFunc &depthFunc;
            const u32 &depthBoundsTestEnable;
            const u32 &stencilTestEnable;
            const u32 &twoSidedStencilTestEnable;
            const engine::StencilOps &stencilOps;
            const engine::StencilOps &stencilBack;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

      public:
        DepthStencilState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(PackedPipelineState &packedState);
    };

    class ColorBlendState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            const engine::LogicOp &logicOp;
            const u32 &singleCtWriteControl;
            const std::array<engine::CtWrite, engine::ColorTargetCount> &ctWrites;
            const u32 &blendStatePerTargetEnable;
            const std::array<engine::BlendPerTarget, engine::ColorTargetCount> &blendPerTargets;
            const engine::Blend &blend;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

      public:
        ColorBlendState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(PackedPipelineState &packedState);
    };

    class GlobalShaderConfigState {
      public:
        struct EngineRegisters {
            const std::array<u32, 8> &postVtgShaderAttributeSkipMask;
            const engine::BindlessTexture &bindlessTexture;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        EngineRegisters engine;

      public:
        GlobalShaderConfigState(const EngineRegisters &engine);

        void Update(PackedPipelineState &packedState);
    };

    /**
     * @brief Holds all GPU state for a pipeline, any changes to this will result in a pipeline cache lookup
     */
    class PipelineState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            std::array<IndividualShaderState::EngineRegisters, engine::PipelineCount> shadersRegisters;
            std::array<ColorRenderTargetState::EngineRegisters, engine::ColorTargetCount> colorRenderTargetsRegisters;
            DepthRenderTargetState::EngineRegisters depthRenderTargetRegisters;
            VertexInputState::EngineRegisters vertexInputRegisters;
            InputAssemblyState::EngineRegisters inputAssemblyRegisters;
            TessellationState::EngineRegisters tessellationRegisters;
            RasterizationState::EngineRegisters rasterizationRegisters;
            DepthStencilState::EngineRegisters depthStencilRegisters;
            ColorBlendState::EngineRegisters colorBlendRegisters;
            GlobalShaderConfigState::EngineRegisters globalShaderConfigRegisters;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        PackedPipelineState packedState{};

        dirty::BoundSubresource<EngineRegisters> engine;

        std::array<dirty::ManualDirtyState<IndividualShaderState>, engine::PipelineCount> shaders;
        std::array<dirty::ManualDirtyState<ColorRenderTargetState>, engine::ColorTargetCount> colorRenderTargets;
        dirty::ManualDirtyState<DepthRenderTargetState> depthRenderTarget;
        dirty::ManualDirtyState<VertexInputState> vertexInput;
        TessellationState tessellation;
        dirty::ManualDirtyState<RasterizationState> rasterization;
        dirty::ManualDirtyState<DepthStencilState> depthStencil;
        dirty::ManualDirtyState<ColorBlendState> colorBlend;
        GlobalShaderConfigState globalShaderConfig;

      public:
        DirectPipelineState directState;

        PipelineState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(InterconnectContext &ctx, StateUpdateBuilder &builder);

        std::shared_ptr<TextureView> GetColorRenderTargetForClear(InterconnectContext &ctx, size_t index);

        std::shared_ptr<TextureView> GetDepthRenderTargetForClear(InterconnectContext &ctx);
    };
}
