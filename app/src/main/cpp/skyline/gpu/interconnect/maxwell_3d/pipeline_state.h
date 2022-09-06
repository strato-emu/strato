// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <boost/container/static_vector.hpp>
#include <gpu/texture/texture.h>
#include "common.h"

namespace skyline::gpu::interconnect::maxwell3d {
    class Key {
      private:
        struct {
            u8 ztFormat : 5; // ZtFormat - 0xA as u8
        };

        std::array<u8, engine::ColorTargetCount> ctFormats; //!< ColorTarget::Format as u8

      public:
        std::array<engine::VertexAttribute, engine::VertexAttributeCount> vertexAttributes;

        void SetCtFormat(size_t index, engine::ColorTarget::Format format) {
            ctFormats[index] = static_cast<u8>(format);
        }

        void SetZtFormat(engine::ZtFormat format) {
            ztFormat = static_cast<u8>(format) - static_cast<u8>(engine::ZtFormat::ZF32);
        }
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

        void Flush(InterconnectContext &ctx, Key &key);
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

        void Flush(InterconnectContext &ctx, Key &key);
    };

    struct VertexInputState {
      private:
        std::array<vk::VertexInputBindingDescription, engine::VertexStreamCount> bindingDescs{
            util::MergeInto<vk::VertexInputBindingDescription, engine::VertexStreamCount>(util::IncrementingT<u32>{})
        };
        std::array<vk::VertexInputBindingDivisorDescriptionEXT, engine::VertexStreamCount> bindingDivisorDescs{
            util::MergeInto<vk::VertexInputBindingDivisorDescriptionEXT, engine::VertexStreamCount>(util::IncrementingT<u32>{})
        };
        std::array<vk::VertexInputAttributeDescription, engine::VertexAttributeCount> attributeDescs{
            util::MergeInto<vk::VertexInputAttributeDescription, engine::VertexAttributeCount>(util::IncrementingT<u32>{})
        };

        boost::container::static_vector<vk::VertexInputBindingDivisorDescriptionEXT, engine::VertexStreamCount> activeBindingDivisorDescs;
        boost::container::static_vector<vk::VertexInputAttributeDescription, engine::VertexAttributeCount> activeAttributeDescs;

      public:
        struct EngineRegisters {
            const std::array<engine::VertexStream, engine::VertexStreamCount> &vertexStreamRegisters;
            const std::array<engine::VertexStreamInstance, engine::VertexStreamCount> &vertexStreamInstanceRegisters;
            const std::array<engine::VertexAttribute, engine::VertexAttributeCount> &vertexAttributesRegisters;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

        vk::StructureChain<vk::PipelineVertexInputStateCreateInfo, vk::PipelineVertexInputDivisorStateCreateInfoEXT> Build(InterconnectContext &ctx, const EngineRegisters &engine);

        void SetStride(u32 index, u32 stride);

        void SetInputRate(u32 index, engine::VertexStreamInstance instance);

        void SetDivisor(u32 index, u32 divisor);

        void SetAttribute(u32 index, engine::VertexAttribute attribute);
    };

    struct InputAssemblyState {
      private:
        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState{};
        engine::DrawTopology currentEngineTopology{};

      public:
        struct EngineRegisters {
            const u32 &primitiveRestartEnable;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };


        const vk::PipelineInputAssemblyStateCreateInfo &Build();

        /**
         * @note Calling this *REQUIRES* manually marking the pipeline as dirty
         */
        void SetPrimitiveTopology(engine::DrawTopology topology);

        engine::DrawTopology GetPrimitiveTopology() const;

        bool NeedsQuadConversion() const;

        void SetPrimitiveRestart(bool enable);
    };

    struct TessellationState {
      private:
        vk::PipelineTessellationStateCreateInfo tessellationState{};

      public:
        struct EngineRegisters {
            const u32 &patchControlPoints;
            const engine::TessellationParameters &tessellationParameters;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

        const vk::PipelineTessellationStateCreateInfo &Build();

        void SetPatchControlPoints(u32 controlPoints);

        void SetParameters(engine::TessellationParameters parameters);
    };

    /**
     * @brief Holds pipeline state that is directly written by the engine code, without using dirty tracking
     */
    struct DirectPipelineState {
        VertexInputState vertexInput;
        InputAssemblyState inputAssembly;
        TessellationState tessellation;
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

        void Flush();
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
        vk::PipelineDepthStencilStateCreateInfo depthStencilState{};

        DepthStencilState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush();
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
        Key key{};

        dirty::BoundSubresource<EngineRegisters> engine;

        std::array<dirty::ManualDirtyState<ColorRenderTargetState>, engine::ColorTargetCount> colorRenderTargets;
        dirty::ManualDirtyState<DepthRenderTargetState> depthRenderTarget;
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
