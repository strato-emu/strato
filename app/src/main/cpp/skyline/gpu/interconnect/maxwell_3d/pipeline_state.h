// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <boost/container/static_vector.hpp>
#include <gpu/texture/texture.h>
#include "common.h"
#include "soc/gm20b/engines/maxwell/types.h"

namespace skyline::gpu::interconnect::maxwell3d {
    struct Key {
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

        void SetCtFormat(size_t index, engine::ColorTarget::Format format) {
            ctFormats[index] = static_cast<u8>(format);
        }

        void SetZtFormat(engine::ZtFormat format) {
            ztFormat = static_cast<u8>(format) - static_cast<u8>(engine::ZtFormat::ZF32);
        }

        void SetVertexBinding(u32 index, engine::VertexStream stream, engine::VertexStreamInstance instance) {
            vertexBindings[index].stride = stream.format.stride;
            vertexBindings[index].instanced = instance.isInstanced;
            vertexBindings[index].enable = stream.format.enable;
            vertexBindings[index].divisor = stream.frequency;
        }

        void SetTessellationParameters(engine::TessellationParameters parameters) {
            domainType = parameters.domainType;
            spacing = parameters.spacing;
            outputPrimitives = parameters.outputPrimitives;
        }

        void SetPolygonMode(engine::PolygonMode mode) {
            polygonMode = static_cast<u8>(static_cast<u32>(mode) - 0x1B00);
        }

        u8 PackCompareFunc(engine::CompareFunc func) {
            u32 val{static_cast<u32>(func)};
            return static_cast<u8>(val >= 0x200 ? (val - 0x200) : (val - 1));
        }

        void SetDepthFunc(engine::CompareFunc func) {
            depthFunc = PackCompareFunc(func);
        }

        u8 PackStencilOp(engine::StencilOps::Op op) {
            switch (op) {
                case engine::StencilOps::Op::OglZero:
                    op = engine::StencilOps::Op::D3DZero;
                    break;
                case engine::StencilOps::Op::OglKeep:
                    op = engine::StencilOps::Op::D3DKeep;
                    break;
                case engine::StencilOps::Op::OglReplace:
                    op = engine::StencilOps::Op::D3DReplace;
                    break;
                case engine::StencilOps::Op::OglIncrSat:
                    op = engine::StencilOps::Op::D3DIncrSat;
                    break;
                case engine::StencilOps::Op::OglDecrSat:
                    op = engine::StencilOps::Op::D3DDecrSat;
                    break;
                case engine::StencilOps::Op::OglInvert:
                    op = engine::StencilOps::Op::D3DInvert;
                    break;
                case engine::StencilOps::Op::OglIncr:
                    op = engine::StencilOps::Op::D3DIncr;
                    break;
                case engine::StencilOps::Op::OglDecr:
                    op = engine::StencilOps::Op::D3DDecr;
                    break;
                default:
                    break;
            }

            return static_cast<u8>(op) - 1;
        }

        StencilOps PackStencilOps(engine::StencilOps ops) {
            return {
                .zPass = PackStencilOp(ops.zPass),
                .fail = PackStencilOp(ops.fail),
                .zFail = PackStencilOp(ops.zFail),
                .func = PackCompareFunc(ops.func),
            };
        }

        void SetStencilOps(engine::StencilOps front, engine::StencilOps back) {
            stencilFront = PackStencilOps(front);
            stencilBack = PackStencilOps(back);
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

        void Flush(Key &key);
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

        void Update(Key &key);

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

        void Update(Key &key);
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

        void Flush(Key &key);
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

        void Flush(Key &key);
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
