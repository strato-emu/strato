// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <boost/container/static_vector.hpp>
#include <gpu/texture/texture.h>
#include "common.h"
#include "soc/gm20b/engines/maxwell/types.h"

namespace skyline::gpu::interconnect::maxwell3d {
    /**
     * @brief Packed struct of pipeline state suitable for use as a map key
     * @note This is heavily based around yuzu's pipeline key with some packing modifications
     * @url https://github.com/yuzu-emu/yuzu/blob/9c701774562ea490296b9cbea3dbd8c096bc4483/src/video_core/renderer_vulkan/fixed_pipeline_state.h#L20
     */
    struct PackedPipelineState {
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
            u8 ztFormat : 5; //!< Use {Set, Get}ZtFormat
            engine::DrawTopology topology : 4;
            bool primitiveRestartEnabled : 1;
            engine::TessellationParameters::DomainType domainType : 2;  //!< Use SetTessellationParameters
            engine::TessellationParameters::Spacing spacing : 2; //!< Use SetTessellationParameters
            engine::TessellationParameters::OutputPrimitives outputPrimitives : 2; //!< Use SetTessellationParameters
            bool rasterizerDiscardEnable : 1;
            u8 polygonMode : 2; //!< Use {Set, Get}PolygonMode
            bool cullModeFront : 1;
            bool cullModeBack : 1;
            bool flipYEnable : 1;
            bool frontFaceClockwise : 1; //!< With Y flip transformation already applied
            bool depthBiasEnable : 1;
            engine::ProvokingVertex::Value provokingVertex : 1;
            bool depthTestEnable : 1;
            bool depthWriteEnable : 1;
            u8 depthFunc : 3; //!< Use {Set, Get}DepthFunc
            bool depthBoundsTestEnable : 1;
            bool stencilTestEnable : 1;
        };

        struct VertexBinding {
            u16 stride : 12;
            bool instanced : 1;
            bool enable : 1;
            u8 _pad_ : 2;
            u32 divisor;
        };
        static_assert(sizeof(VertexBinding) == 0x8);

        u32 patchSize;
        std::array<u8, engine::ColorTargetCount> ctFormats; //!< Use {Set, Get}CtFormat
        std::array<VertexBinding, engine::VertexStreamCount> vertexBindings; //!< Use {Set, Get}VertexBinding
        std::array<engine::VertexAttribute, engine::VertexAttributeCount> vertexAttributes;

        void SetCtFormat(size_t index, engine::ColorTarget::Format format);

        void SetZtFormat(engine::ZtFormat format);

        void SetVertexBinding(u32 index, engine::VertexStream stream, engine::VertexStreamInstance instance);

        void SetTessellationParameters(engine::TessellationParameters parameters);

        void SetPolygonMode(engine::PolygonMode mode);

        void SetDepthFunc(engine::CompareFunc func);

        void SetStencilOps(engine::StencilOps front, engine::StencilOps back);
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

    class ColorBlendState : dirty::RefreshableManualDirty {
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
        std::array<vk::PipelineColorBlendAttachmentState, engine::ColorTargetCount> attachmentBlendStates;

      public:
        vk::PipelineColorBlendStateCreateInfo colorBlendState{};

        ColorBlendState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(InterconnectContext &ctx, size_t attachmentCount);

        void Refresh(InterconnectContext &ctx, size_t attachmentCount);
    };

    /**
     * @brief Holds all GPU state for a pipeline, any changes to this will result in a pipeline cache lookup
     */
    class PipelineState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            std::array<ColorRenderTargetState::EngineRegisters, engine::ColorTargetCount> colorRenderTargetsRegisters;
            DepthRenderTargetState::EngineRegisters depthRenderTargetRegisters;
            VertexInputState::EngineRegisters vertexInputRegisters;
            InputAssemblyState::EngineRegisters inputAssemblyRegisters;
            TessellationState::EngineRegisters tessellationRegisters;
            RasterizationState::EngineRegisters rasterizationRegisters;
            DepthStencilState::EngineRegisters depthStencilRegisters;
            ColorBlendState::EngineRegisters colorBlendRegisters;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        PackedPipelineState packedState{};

        dirty::BoundSubresource<EngineRegisters> engine;

        std::array<dirty::ManualDirtyState<ColorRenderTargetState>, engine::ColorTargetCount> colorRenderTargets;
        dirty::ManualDirtyState<DepthRenderTargetState> depthRenderTarget;
        dirty::ManualDirtyState<VertexInputState> vertexInput;
        TessellationState tessellation;
        dirty::ManualDirtyState<RasterizationState> rasterization;
        dirty::ManualDirtyState<DepthStencilState> depthStencil;
        dirty::ManualDirtyState<ColorBlendState> colorBlend;


      public:
        DirectPipelineState directState;

        PipelineState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(InterconnectContext &ctx, StateUpdateBuilder &builder);

        std::shared_ptr<TextureView> GetColorRenderTargetForClear(InterconnectContext &ctx, size_t index);

        std::shared_ptr<TextureView> GetDepthRenderTargetForClear(InterconnectContext &ctx);
    };
}
