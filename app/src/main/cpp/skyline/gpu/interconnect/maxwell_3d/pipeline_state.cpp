// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/Ryujinx/)
// Copyright © 2022 yuzu Team and Contributors (https://github.com/yuzu-emu/)
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <range/v3/algorithm/for_each.hpp>
#include <nce.h>
#include <kernel/memory.h>
#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include <gpu/texture/format.h>
#include <gpu.h>
#include "pipeline_state.h"

namespace skyline::gpu::interconnect::maxwell3d {
    static void DetermineRenderTargetDimensions(GuestTexture &guest, const engine::SurfaceClip &clip) {
        // RT dimensions always include block linear alignment and contain the unaligned dimensions in surface clip, we ideally want to create the texture using the unaligned dimensions since the texture manager doesn't support resolving such overlaps yet. By checking that the layer size calculated is equal to the RT size we can eliminate most cases where the clip is used not just for aligmnent
        u32 underlyingRtLayerSize{guest.CalculateLayerSize()};
        texture::Dimensions underlyingRtDimensions{guest.dimensions};
        guest.dimensions = texture::Dimensions{static_cast<u32>(clip.horizontal.width + clip.horizontal.x),
                                               static_cast<u32>(clip.vertical.height + clip.vertical.y),
                                               guest.dimensions.depth};
        u32 clippedRtLayerSize{guest.CalculateLayerSize()};

        // If the calculated sizes don't match then always use the RT dimensions
        if (clippedRtLayerSize != underlyingRtLayerSize)
            guest.dimensions = underlyingRtDimensions;
    }

    /* Colour Render Target */
    void ColorRenderTargetState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, colorTarget, surfaceClip);
    }

    ColorRenderTargetState::ColorRenderTargetState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine, size_t index) : engine{manager, dirtyHandle, engine}, index{index} {}

    static texture::Format ConvertColorRenderTargetFormat(engine::ColorTarget::Format format) {
        #define FORMAT_CASE_BASE(engineFormat, skFormat, warn) \
                case engine::ColorTarget::Format::engineFormat:                     \
                    if constexpr (warn)                                             \
                        Logger::Warn("Partially supported RT format: " #engineFormat " used!"); \
                    return skyline::gpu::format::skFormat

        #define FORMAT_CASE(engineFormat, skFormat) FORMAT_CASE_BASE(engineFormat, skFormat, false)
        #define FORMAT_CASE_WARN(engineFormat, skFormat) FORMAT_CASE_BASE(engineFormat, skFormat, true)

        switch (format) {
            FORMAT_CASE(RF32_GF32_BF32_AF32, R32G32B32A32Float);
            FORMAT_CASE(RS32_GS32_BS32_AS32, R32G32B32A32Sint);
            FORMAT_CASE(RU32_GU32_BU32_AU32, R32G32B32A32Uint);
            FORMAT_CASE_WARN(RF32_GF32_BF32_X32, R32G32B32A32Float); // TODO: ignore X32 component with blend
            FORMAT_CASE_WARN(RS32_GS32_BS32_X32, R32G32B32A32Sint); // TODO: ^
            FORMAT_CASE_WARN(RU32_GU32_BU32_X32, R32G32B32A32Uint); // TODO: ^
            FORMAT_CASE(R16_G16_B16_A16, R16G16B16A16Unorm);
            FORMAT_CASE(RN16_GN16_BN16_AN16, R16G16B16A16Snorm);
            FORMAT_CASE(RS16_GS16_BS16_AS16, R16G16B16A16Sint);
            FORMAT_CASE(RU16_GU16_BU16_AU16, R16G16B16A16Uint);
            FORMAT_CASE(RF16_GF16_BF16_AF16, R16G16B16A16Float);
            FORMAT_CASE(RF32_GF32, R32G32Float);
            FORMAT_CASE(RS32_GS32, R32G32Sint);
            FORMAT_CASE(RU32_GU32, R32G32Uint);
            FORMAT_CASE_WARN(RF16_GF16_BF16_X16, R16G16B16A16Float); // TODO: ^^
            FORMAT_CASE(A8R8G8B8, B8G8R8A8Unorm);
            FORMAT_CASE(A8RL8GL8BL8, B8G8R8A8Srgb);
            FORMAT_CASE(A2B10G10R10, A2B10G10R10Unorm);
            FORMAT_CASE(AU2BU10GU10RU10, A2B10G10R10Uint);
            FORMAT_CASE(A8B8G8R8, R8G8B8A8Unorm);
            FORMAT_CASE(A8BL8GL8RL8, R8G8B8A8Srgb);
            FORMAT_CASE(AN8BN8GN8RN8, R8G8B8A8Snorm);
            FORMAT_CASE(AS8BS8GS8RS8, R8G8B8A8Sint);
            FORMAT_CASE(AU8BU8GU8RU8, R8G8B8A8Uint);
            FORMAT_CASE(R16_G16, R16G16Unorm);
            FORMAT_CASE(RN16_GN16, R16G16Snorm);
            FORMAT_CASE(RS16_GS16, R16G16Sint);
            FORMAT_CASE(RU16_GU16, R16G16Uint);
            FORMAT_CASE(RF16_GF16, R16G16Float);
            FORMAT_CASE(A2R10G10B10, A2B10G10R10Unorm);
            FORMAT_CASE(BF10GF11RF11, B10G11R11Float);
            FORMAT_CASE(RS32, R32Sint);
            FORMAT_CASE(RU32, R32Uint);
            FORMAT_CASE(RF32, R32Float);
            FORMAT_CASE_WARN(X8R8G8B8, B8G8R8A8Unorm); // TODO: ^^
            FORMAT_CASE_WARN(X8RL8GL8BL8, B8G8R8A8Srgb); // TODO: ^^
            FORMAT_CASE(R5G6B5, R5G6B5Unorm);
            FORMAT_CASE(A1R5G5B5, A1R5G5B5Unorm);
            FORMAT_CASE(G8R8, R8G8Unorm);
            FORMAT_CASE(GN8RN8, R8G8Snorm);
            FORMAT_CASE(GS8RS8, R8G8Sint);
            FORMAT_CASE(GU8RU8, R8G8Uint);
            FORMAT_CASE(R16, R16Unorm);
            FORMAT_CASE(RN16, R16Snorm);
            FORMAT_CASE(RS16, R16Sint);
            FORMAT_CASE(RU16, R16Uint);
            FORMAT_CASE(RF16, R16Float);
            FORMAT_CASE(R8, R8Unorm);
            FORMAT_CASE(RN8, R8Snorm);
            FORMAT_CASE(RS8, R8Sint);
            FORMAT_CASE(RU8, R8Uint);
            // FORMAT_CASE(A8, A8Unorm);
            FORMAT_CASE_WARN(X1R5G5B5, A1R5G5B5Unorm); // TODO: ^^
            FORMAT_CASE_WARN(X8B8G8R8, R8G8B8A8Unorm); // TODO: ^^
            FORMAT_CASE_WARN(X8BL8GL8RL8, R8G8B8A8Srgb); // TODO: ^^
            FORMAT_CASE_WARN(Z1R5G5B5, A1R5G5B5Unorm); // TODO: ^^ but with zero blend
            FORMAT_CASE_WARN(O1R5G5B5, A1R5G5B5Unorm); // TODO: ^^ but with one blend
            FORMAT_CASE_WARN(Z8R8G8B8, B8G8R8A8Unorm); // TODO: ^^ but with zero blend
            FORMAT_CASE_WARN(O8R8G8B8, B8G8R8A8Unorm); // TODO: ^^ but with one blend
            // FORMAT_CASE(R32, R32Unorm);
            // FORMAT_CASE(A16, A16Unorm);
            // FORMAT_CASE(AF16, A16Float);
            // FORMAT_CASE(AF32, A32Float);
            // FORMAT_CASE(A8R8, R8A8Unorm);
            // FORMAT_CASE(R16_A16, R16A16Unorm);
            // FORMAT_CASE(RF16_AF16, R16A16Float);
            // FORMAT_CASE(RF32_AF32, R32A32Float);
            // FORMAT_CASE(B8G8R8A8, A8R8G8B8Unorm)
            default:
                throw exception("Unsupported colour rendertarget format: 0x{:X}", static_cast<u32>(format));
        }

        #undef FORMAT_CASE
        #undef FORMAT_CASE_WARN
        #undef FORMAT_CASE_BASE
    }

    void ColorRenderTargetState::Flush(InterconnectContext &ctx, PackedPipelineState &packedState) {
        auto &target{engine->colorTarget};
        packedState.SetColorRenderTargetFormat(index, target.format);

        if (target.format == engine::ColorTarget::Format::Disabled) {
            view = {};
            return;
        }

        GuestTexture guest{};
        guest.format = ConvertColorRenderTargetFormat(target.format);
        guest.aspect = vk::ImageAspectFlagBits::eColor;
        guest.baseArrayLayer = target.layerOffset;

        bool thirdDimensionDefinesArraySize{target.memory.thirdDimensionControl == engine::TargetMemory::ThirdDimensionControl::ThirdDimensionDefinesArraySize};
        guest.layerCount = thirdDimensionDefinesArraySize ? target.thirdDimension : 1;
        guest.viewType = target.thirdDimension > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D;

        u32 depth{thirdDimensionDefinesArraySize ? 1U : target.thirdDimension};
        if (target.memory.layout == engine::TargetMemory::Layout::Pitch) {
            guest.dimensions = texture::Dimensions{target.width / guest.format->bpb, target.height, depth};
            guest.tileConfig = texture::TileConfig{
                .mode = gpu::texture::TileMode::Pitch,
                .pitch = target.width,
            };
        } else {
            guest.dimensions = gpu::texture::Dimensions{target.width, target.height, depth};
            guest.tileConfig = gpu::texture::TileConfig{
                .mode = gpu::texture::TileMode::Block,
                .blockHeight = target.memory.BlockHeight(),
                .blockDepth = target.memory.BlockDepth(),
            };
        }

        guest.layerStride = (guest.baseArrayLayer > 1 || guest.layerCount > 1) ? target.ArrayPitch() : 0;

        auto mappings{ctx.channelCtx.asCtx->gmmu.TranslateRange(target.offset, guest.GetSize())};
        guest.mappings.assign(mappings.begin(), mappings.end());

        if (guest.MappingsValid()) {
            if (guest.tileConfig.mode == gpu::texture::TileMode::Block)
                DetermineRenderTargetDimensions(guest, engine->surfaceClip);

            view = ctx.gpu.texture.FindOrCreate(guest, ctx.executor.tag);
        } else {
            view = {};
        }
    }

    /* Depth Render Target */
    void DepthRenderTargetState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, ztSize, ztOffset, ztFormat, ztBlockSize, ztArrayPitch, ztSelect, ztLayer, surfaceClip);
    }

    DepthRenderTargetState::DepthRenderTargetState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    static texture::Format ConvertDepthRenderTargetFormat(engine::ZtFormat format) {
        #define FORMAT_CASE(engineFormat, skFormat) \
            case engine::ZtFormat::engineFormat: \
                return skyline::gpu::format::skFormat

        switch (format) {
            FORMAT_CASE(Z16, D16Unorm);
            FORMAT_CASE(Z24S8, S8UintD24Unorm);
            FORMAT_CASE(X8Z24, D24UnormX8Uint);
            FORMAT_CASE(S8Z24, D24UnormS8Uint);
            FORMAT_CASE(S8, S8Uint);
            FORMAT_CASE(ZF32, D32Float);
            FORMAT_CASE(ZF32_X24S8, D32FloatS8Uint);
            default:
                throw exception("Unsupported depth rendertarget format: 0x{:X}", static_cast<u32>(format));
        }

        #undef FORMAT_CASE
    }

    void DepthRenderTargetState::Flush(InterconnectContext &ctx, PackedPipelineState &packedState) {
        packedState.SetDepthRenderTargetFormat(engine->ztFormat);

        if (!engine->ztSelect.targetCount) {
            view = {};
            return;
        }

        GuestTexture guest{};
        guest.format = ConvertDepthRenderTargetFormat(engine->ztFormat);
        guest.aspect = guest.format->vkAspect;
        guest.baseArrayLayer = engine->ztLayer.offset;

        bool thirdDimensionDefinesArraySize{engine->ztSize.control == engine::ZtSize::Control::ThirdDimensionDefinesArraySize};
        if (engine->ztSize.control == engine::ZtSize::Control::ThirdDimensionDefinesArraySize) {
            guest.layerCount = engine->ztSize.thirdDimension;
            guest.viewType = engine->ztSize.thirdDimension > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D;
        } else if (engine->ztSize.control == engine::ZtSize::Control::ArraySizeIsOne) {
            guest.layerCount = 1;
            guest.viewType = vk::ImageViewType::e2D;
        }

        guest.dimensions = gpu::texture::Dimensions{engine->ztSize.width, engine->ztSize.height, 1};
        guest.tileConfig = gpu::texture::TileConfig{
            .mode = gpu::texture::TileMode::Block,
            .blockHeight = engine->ztBlockSize.BlockHeight(),
            .blockDepth = engine->ztBlockSize.BlockDepth(),
        };

        guest.layerStride = (guest.baseArrayLayer > 1 || guest.layerCount > 1) ? engine->ztArrayPitch : 0;

        auto mappings{ctx.channelCtx.asCtx->gmmu.TranslateRange(engine->ztOffset, guest.GetSize())};
        guest.mappings.assign(mappings.begin(), mappings.end());

        if (guest.MappingsValid()) {
            if (guest.tileConfig.mode == gpu::texture::TileMode::Block)
                DetermineRenderTargetDimensions(guest, engine->surfaceClip);

            view = ctx.gpu.texture.FindOrCreate(guest, ctx.executor.tag);
        } else {
            view = {};
        }
    }

    /* Pipeline Stages */
    void PipelineStageState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, pipeline, programRegion);
    }

    PipelineStageState::PipelineStageState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine, u8 shaderType)
        : engine{manager, dirtyHandle, engine},
          shaderType{static_cast<engine::Pipeline::Shader::Type>(shaderType)} {}

    void PipelineStageState::Flush(InterconnectContext &ctx) {
        if (engine->pipeline.shader.type != shaderType)
            throw exception("Shader type mismatch: {} != {}!", engine->pipeline.shader.type, static_cast<u8>(shaderType));

        if (!engine->pipeline.shader.enable && shaderType != engine::Pipeline::Shader::Type::Vertex) {
            binary.hash = 0;
            return;
        }

        binary = cache.Lookup(ctx, engine->programRegion, engine->pipeline.programOffset);
    }

    bool PipelineStageState::Refresh(InterconnectContext &ctx) {
        return cache.Refresh(ctx, engine->programRegion, engine->pipeline.programOffset);
    }

    void PipelineStageState::PurgeCaches() {
        cache.PurgeCaches();
    }

    /* Vertex Input State */
    void VertexInputState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        ranges::for_each(vertexStreams, [&](const auto &regs) { manager.Bind(handle, regs.format, regs.frequency); });

        auto bindFull{[&](const auto &regs) { manager.Bind(handle, regs); }};
        ranges::for_each(vertexStreamInstance, bindFull);
        ranges::for_each(vertexAttributes, bindFull);
    }

    VertexInputState::VertexInputState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    void VertexInputState::Flush(PackedPipelineState &packedState) {
        for (u32 i{}; i < engine::VertexStreamCount; i++)
            packedState.SetVertexBinding(i, engine->vertexStreams[i], engine->vertexStreamInstance[i]);

        for (u32 i{}; i < engine::VertexAttributeCount; i++) {
            if (engine->vertexAttributes[i].source == engine::VertexAttribute::Source::Active)
                packedState.vertexAttributes[i] = engine->vertexAttributes[i];
            else
                packedState.vertexAttributes[i] = { .source = engine::VertexAttribute::Source::Inactive };
        }
    }

    /* Input Assembly State */
    void InputAssemblyState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, primitiveRestartEnable);
    }

    InputAssemblyState::InputAssemblyState(const EngineRegisters &engine) : engine{engine} {}

    void InputAssemblyState::Update(PackedPipelineState &packedState) {
        packedState.topology = currentEngineTopology;
        packedState.primitiveRestartEnabled = engine.primitiveRestartEnable & 1;
    }

    void InputAssemblyState::SetPrimitiveTopology(engine::DrawTopology topology) {
        currentEngineTopology = topology;
    }

    engine::DrawTopology InputAssemblyState::GetPrimitiveTopology() const {
        return currentEngineTopology;
    }

    bool InputAssemblyState::NeedsQuadConversion() const {
        return currentEngineTopology == engine::DrawTopology::Quads;
    }

    /* Tessellation State */
    void TessellationState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, patchSize, tessellationParameters);
    }

    TessellationState::TessellationState(const EngineRegisters &engine) : engine{engine} {}

    void TessellationState::Update(PackedPipelineState &packedState) {
        packedState.patchSize = engine.patchSize;
        packedState.SetTessellationParameters(engine.tessellationParameters);
    }

    /* Rasterizer State */
    void RasterizationState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, rasterEnable, frontPolygonMode, backPolygonMode, viewportClipControl, oglCullEnable, oglFrontFace, oglCullFace, windowOrigin, provokingVertex, polyOffset, pointSize, zClipRange);
    }

    RasterizationState::RasterizationState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    bool ConvertDepthBiasEnable(engine::PolyOffset polyOffset, engine::PolygonMode polygonMode) {
        switch (polygonMode) {
            case engine::PolygonMode::Point:
                return polyOffset.pointEnable;
            case engine::PolygonMode::Line:
                return polyOffset.lineEnable;
            case engine::PolygonMode::Fill:
                return polyOffset.fillEnable;
            default:
                throw exception("Invalid polygon mode: 0x{:X}", static_cast<u32>(polygonMode));
        }
    }

    void RasterizationState::Flush(PackedPipelineState &packedState) {
        packedState.rasterizerDiscardEnable = !engine->rasterEnable;
        packedState.SetPolygonMode(engine->frontPolygonMode);
        if (engine->backPolygonMode != engine->frontPolygonMode)
            Logger::Warn("Non-matching polygon modes!");

        packedState.SetCullMode(engine->oglCullEnable, engine->oglCullFace);

        packedState.flipYEnable = engine->windowOrigin.flipY;

        bool origFrontFaceClockwise{engine->oglFrontFace == engine::FrontFace::CW};
        packedState.frontFaceClockwise = (packedState.flipYEnable != origFrontFaceClockwise);
        packedState.depthBiasEnable = ConvertDepthBiasEnable(engine->polyOffset, engine->frontPolygonMode);
        packedState.provokingVertex = engine->provokingVertex.value;
        packedState.pointSize = engine->pointSize;
        packedState.openGlNdc = engine->zClipRange == engine::ZClipRange::NegativeWToPositiveW;
        packedState.SetDepthClampEnable(engine->viewportClipControl.geometryClip);
    }

    /* Depth Stencil State */
    void DepthStencilState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, depthTestEnable, depthWriteEnable, depthFunc, depthBoundsTestEnable, stencilTestEnable, twoSidedStencilTestEnable, stencilOps, stencilBack, alphaTestEnable, alphaFunc, alphaRef);
    }

    DepthStencilState::DepthStencilState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    void DepthStencilState::Flush(PackedPipelineState &packedState) {
        packedState.depthTestEnable = engine->depthTestEnable;
        packedState.depthWriteEnable = engine->depthWriteEnable;
        packedState.SetDepthFunc(engine->depthTestEnable ? engine->depthFunc : engine::CompareFunc::OglAlways);
        packedState.depthBoundsTestEnable = engine->depthBoundsTestEnable;

        packedState.stencilTestEnable = engine->stencilTestEnable;
        if (packedState.stencilTestEnable) {
            auto stencilBack{engine->twoSidedStencilTestEnable ? engine->stencilBack : engine->stencilOps};
            packedState.SetStencilOps(engine->stencilOps, stencilBack);
        } else {
            packedState.SetStencilOps({ .func = engine::CompareFunc::OglAlways }, { .func = engine::CompareFunc::OglAlways });
        }

        packedState.alphaTestEnable = engine->alphaTestEnable;
        packedState.SetAlphaFunc(engine->alphaTestEnable ? engine->alphaFunc : engine::CompareFunc::OglAlways);
        packedState.alphaRef = engine->alphaTestEnable ? engine->alphaRef : 0;
    };

    /* Color Blend State */
    void ColorBlendState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, logicOp, singleCtWriteControl, ctWrites, blendStatePerTargetEnable, blendPerTargets, blend);
    }

    ColorBlendState::ColorBlendState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    void ColorBlendState::Flush(PackedPipelineState &packedState) {
        packedState.logicOpEnable = engine->logicOp.enable;
        packedState.SetLogicOp(engine->logicOp.func);

        for (u32 i{}; i < engine::ColorTargetCount; i++) {
            auto ctWrite{[&]() {
                if (!packedState.activeColorTargets.test(i))
                    return engine::CtWrite{};

                if (engine->singleCtWriteControl)
                    return engine->ctWrites[0];
                else
                    return engine->ctWrites[i];
            }()};

            bool enable{engine->blend.enable[i] != 0 && packedState.activeColorTargets.test(i)};

            if (engine->blendStatePerTargetEnable)
                packedState.SetAttachmentBlendState(i, enable, ctWrite, engine->blendPerTargets[i]);
            else
                packedState.SetAttachmentBlendState(i, enable, ctWrite, engine->blend);
        }
    }

    /* Transform Feedback State */
    void TransformFeedbackState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, streamOutputEnable, streamOutControls, streamOutLayoutSelect);
    }

    TransformFeedbackState::TransformFeedbackState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    void TransformFeedbackState::Flush(PackedPipelineState &packedState) {
        packedState.transformFeedbackEnable = engine->streamOutputEnable;
        packedState.transformFeedbackVaryings = {};

        if (engine->streamOutputEnable)
            for (size_t i{}; i < engine::StreamOutBufferCount; i++)
                packedState.SetTransformFeedbackVaryings(engine->streamOutControls[i], engine->streamOutLayoutSelect[i], i);
    }

    /* Global Shader Config State */
    void GlobalShaderConfigState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, postVtgShaderAttributeSkipMask, bindlessTexture, apiMandatedEarlyZ, viewportScaleOffsetEnable);
    }

    GlobalShaderConfigState::GlobalShaderConfigState(const EngineRegisters &engine) : engine{engine} {}

    void GlobalShaderConfigState::Update(PackedPipelineState &packedState) {
        packedState.postVtgShaderAttributeSkipMask = engine.postVtgShaderAttributeSkipMask;
        packedState.bindlessTextureConstantBufferSlotSelect = engine.bindlessTexture.constantBufferSlotSelect;
        packedState.apiMandatedEarlyZ = engine.apiMandatedEarlyZ;
        packedState.viewportTransformEnable = engine.viewportScaleOffsetEnable;
    }

    /* Pipeline State */
    void PipelineState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        auto bindFunc{[&](auto &regs) { regs.DirtyBind(manager, handle); }};

        ranges::for_each(pipelineStageRegisters, bindFunc);
        ranges::for_each(colorRenderTargetsRegisters, bindFunc);
        bindFunc(depthRenderTargetRegisters);
        bindFunc(vertexInputRegisters);
        bindFunc(inputAssemblyRegisters);
        bindFunc(tessellationRegisters);
        bindFunc(rasterizationRegisters);
        bindFunc(depthStencilRegisters);
        bindFunc(colorBlendRegisters);
        bindFunc(globalShaderConfigRegisters);
        bindFunc(transformFeedbackRegisters);
        manager.Bind(handle, ctSelect);
    }

    PipelineState::PipelineState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine)
        : engine{manager, dirtyHandle, engine},
          pipelineStages{util::MergeInto<dirty::ManualDirtyState<PipelineStageState>, engine::PipelineCount>(manager, engine.pipelineStageRegisters, util::IncrementingT<u8>{})},
          colorRenderTargets{util::MergeInto<dirty::ManualDirtyState<ColorRenderTargetState>, engine::ColorTargetCount>(manager, engine.colorRenderTargetsRegisters, util::IncrementingT<size_t>{})},
          depthRenderTarget{manager, engine.depthRenderTargetRegisters},
          vertexInput{manager, engine.vertexInputRegisters},
          tessellation{engine.tessellationRegisters},
          rasterization{manager, engine.rasterizationRegisters},
          depthStencil{manager, engine.depthStencilRegisters},
          colorBlend{manager, engine.colorBlendRegisters},
          transformFeedback{manager, engine.transformFeedbackRegisters},
          directState{engine.inputAssemblyRegisters},
          globalShaderConfig{engine.globalShaderConfigRegisters},
          ctSelect{engine.ctSelect} {}

    void PipelineState::Flush(InterconnectContext &ctx, Textures &textures, ConstantBufferSet &constantBuffers, StateUpdateBuilder &builder) {
        packedState.dynamicStateActive = ctx.gpu.traits.supportsExtendedDynamicState;

        std::array<ShaderBinary, engine::PipelineCount> shaderBinaries;
        for (size_t i{}; i < engine::PipelineCount; i++) {
            const auto &stage{pipelineStages[i].UpdateGet(ctx)};
            packedState.shaderHashes[i] = stage.binary.hash;
            shaderBinaries[i] = stage.binary;
        }

        colorAttachments.clear();
        packedState.activeColorTargets.reset();
        for (size_t i{}; i < ctSelect.count; i++) {
            const auto &view{colorRenderTargets[ctSelect[i]].UpdateGet(ctx, packedState).view.get()};
            colorAttachments.push_back(view);
            packedState.activeColorTargets.set(i);

            if (view)
                ctx.executor.AttachTexture(view);
        }

        depthAttachment = depthRenderTarget.UpdateGet(ctx, packedState).view.get();
        if (depthAttachment)
            ctx.executor.AttachTexture(depthAttachment);

        vertexInput.Update(packedState);
        directState.inputAssembly.Update(packedState);
        tessellation.Update(packedState);
        rasterization.Update(packedState);
        depthStencil.Update(packedState);
        colorBlend.Update(packedState);
        transformFeedback.Update(packedState);
        globalShaderConfig.Update(packedState);

        if (pipeline) {
            if (auto newPipeline{pipeline->LookupNext(packedState)}) {
                pipeline = newPipeline;
                return;
            }
        }

        auto newPipeline{pipelineManager.FindOrCreate(ctx, textures, constantBuffers, packedState, shaderBinaries, colorAttachments, depthAttachment)};
        if (pipeline)
            pipeline->AddTransition(newPipeline);
        pipeline = newPipeline;
    }

    void PipelineState::PurgeCaches() {
        pipeline = nullptr;
        for (auto &stage : pipelineStages)
            stage.MarkDirty(true);
    }

    std::shared_ptr<TextureView> PipelineState::GetColorRenderTargetForClear(InterconnectContext &ctx, size_t index) {
        return colorRenderTargets[index].UpdateGet(ctx, packedState).view;
    }

    std::shared_ptr<TextureView> PipelineState::GetDepthRenderTargetForClear(InterconnectContext &ctx) {
        return depthRenderTarget.UpdateGet(ctx, packedState).view;
    }
}
