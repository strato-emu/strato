// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <gpu/buffer_manager.h>
#include "common.h"
#include "pipeline_state.h"

namespace skyline::gpu::interconnect::maxwell3d {
    class VertexBufferState : dirty::RefreshableManualDirty, dirty::CachedManualDirty {
      public:
        struct EngineRegisters {
            const engine::VertexStream &vertexStream;
            const soc::gm20b::engine::Address &vertexStreamLimit;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

        CachedMappedBufferView view{};
        BufferBinding megaBufferBinding{};
        u32 index{};

      public:
        VertexBufferState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine, u32 index);

        void Flush(InterconnectContext &ctx, StateUpdateBuilder &builder, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask);

        bool Refresh(InterconnectContext &ctx, StateUpdateBuilder &builder, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask);

        void PurgeCaches();
    };

    class IndexBufferState : dirty::RefreshableManualDirty, dirty::CachedManualDirty {
      public:
        struct EngineRegisters {
            const engine::IndexBuffer &indexBuffer;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;
        CachedMappedBufferView view{};
        BufferBinding megaBufferBinding{};
        vk::IndexType indexType{};
        bool didEstimateSize{};
        u32 usedElementCount{};
        u32 usedFirstIndex{};
        bool usedQuadConversion{};

      public:
        IndexBufferState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(InterconnectContext &ctx, StateUpdateBuilder &builder, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask, bool quadConversion, bool estimateSize, u32 firstIndex, u32 elementCount);

        bool Refresh(InterconnectContext &ctx, StateUpdateBuilder &builder, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask, bool quadConversion, bool estimateSize, u32 firstIndex, u32 elementCount);

        void PurgeCaches();
    };

    class TransformFeedbackBufferState : dirty::CachedManualDirty, dirty::RefreshableManualDirty {
      public:
        struct EngineRegisters {
            const engine::StreamOutBuffer &streamOutBuffer;
            const u32 &streamOutEnable;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;
        CachedMappedBufferView view{};
        u32 index{};

      public:
        TransformFeedbackBufferState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine, u32 index);

        void Flush(InterconnectContext &ctx, StateUpdateBuilder &builder, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask);

        bool Refresh(InterconnectContext &ctx, StateUpdateBuilder &builder, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask);

        void PurgeCaches();
    };

    class ViewportState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            const engine::Viewport &viewport0;
            const engine::ViewportClip &viewportClip0;
            const engine::Viewport &viewport;
            const engine::ViewportClip &viewportClip;
            const engine::WindowOrigin &windowOrigin;
            const u32 &viewportScaleOffsetEnable;
            const engine::SurfaceClip &surfaceClip;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;
        u32 index{};

      public:
        ViewportState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine, u32 index);

        void Flush(InterconnectContext &ctx, StateUpdateBuilder &builder);
    };

    class ScissorState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            const engine::Scissor &scissor;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;
        u32 index;

      public:
        ScissorState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine, u32 index);

        void Flush(InterconnectContext &ctx, StateUpdateBuilder &builder);
    };

    struct LineWidthState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            const float &lineWidth;
            const float &lineWidthAliased;
            const u32 &aliasedLineWidthEnable;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

      public:
        LineWidthState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(InterconnectContext &ctx, StateUpdateBuilder &builder);
    };

    struct DepthBiasState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            const float &depthBias;
            const float &depthBiasClamp;
            const float &slopeScaleDepthBias;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

      public:
        DepthBiasState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(InterconnectContext &ctx, StateUpdateBuilder &builder);
    };

    struct BlendConstantsState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            const std::array<float, engine::BlendColorChannelCount> &blendConsts;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

      public:
        BlendConstantsState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(InterconnectContext &ctx, StateUpdateBuilder &builder);
    };

    class DepthBoundsState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            const float &depthBoundsMin;
            const float &depthBoundsMax;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

      public:
        DepthBoundsState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(InterconnectContext &ctx, StateUpdateBuilder &builder);
    };

    class StencilValuesState : dirty::ManualDirty {
      public:
        struct EngineRegisters {
            const engine::StencilValues &stencilValues;
            const engine::BackStencilValues &backStencilValues;
            const u32 &twoSidedStencilTestEnable;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

      public:
        StencilValuesState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(InterconnectContext &ctx, StateUpdateBuilder &builder);
    };

    /**
     * @brief Holds all GPU state that can be dynamically updated without changing the active pipeline
     */
    class ActiveState {
      private:
        dirty::ManualDirtyState<PipelineState> pipeline;
        std::array<dirty::ManualDirtyState<VertexBufferState>, engine::VertexStreamCount> vertexBuffers;
        dirty::ManualDirtyState<IndexBufferState> indexBuffer;
        std::array<dirty::ManualDirtyState<TransformFeedbackBufferState>, engine::StreamOutBufferCount> transformFeedbackBuffers;
        std::array<dirty::ManualDirtyState<ViewportState>, engine::ViewportCount> viewports;
        std::array<dirty::ManualDirtyState<ScissorState>, engine::ViewportCount> scissors;
        dirty::ManualDirtyState<LineWidthState> lineWidth;
        dirty::ManualDirtyState<DepthBiasState> depthBias;
        dirty::ManualDirtyState<BlendConstantsState> blendConstants;
        dirty::ManualDirtyState<DepthBoundsState> depthBounds;
        dirty::ManualDirtyState<StencilValuesState> stencilValues;

      public:
        struct EngineRegisters {
            PipelineState::EngineRegisters pipelineRegisters;
            std::array<VertexBufferState::EngineRegisters, engine::VertexStreamCount> vertexBuffersRegisters;
            IndexBufferState::EngineRegisters indexBufferRegisters;
            std::array<TransformFeedbackBufferState::EngineRegisters, engine::StreamOutBufferCount> transformFeedbackBuffersRegisters;
            std::array<ViewportState::EngineRegisters, engine::ViewportCount> viewportsRegisters;
            std::array<ScissorState::EngineRegisters, engine::ViewportCount> scissorsRegisters;
            LineWidthState::EngineRegisters lineWidthRegisters;
            DepthBiasState::EngineRegisters depthBiasRegisters;
            BlendConstantsState::EngineRegisters blendConstantsRegisters;
            DepthBoundsState::EngineRegisters depthBoundsRegisters;
            StencilValuesState::EngineRegisters stencilValuesRegisters;
        };

        DirectPipelineState &directState;

        ActiveState(DirtyManager &manager, const EngineRegisters &engineRegisters);

        void MarkAllDirty();

        /**
         * @brief Updates the active state for a given draw operation, removing the dirtiness of all member states
         * @note If `extimateIndexBufferSize` is false and `indexed` is true the `drawFirstIndex` and `drawElementCount` arguments must be populated
         */
        void Update(InterconnectContext &ctx, Textures &textures, ConstantBufferSet &constantBuffers, StateUpdateBuilder &builder,
                    bool indexed, engine::DrawTopology topology, bool estimateIndexBufferSize, u32 drawFirstIndex, u32 drawElementCount,
                    vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask);

        Pipeline *GetPipeline();

        span<TextureView *> GetColorAttachments();

        TextureView *GetDepthAttachment();

        std::shared_ptr<TextureView> GetColorRenderTargetForClear(InterconnectContext &ctx, size_t index);

        std::shared_ptr<TextureView> GetDepthRenderTargetForClear(InterconnectContext &ctx);
    };
}
