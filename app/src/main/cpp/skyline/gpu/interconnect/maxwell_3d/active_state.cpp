// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/Ryujinx/)
// Copyright © 2022 yuzu Team and Contributors (https://github.com/yuzu-emu/)
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <limits>
#include <range/v3/algorithm.hpp>
#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include <gpu/buffer_manager.h>
#include <gpu/interconnect/command_executor.h>
#include <gpu/interconnect/conversion/quads.h>
#include <gpu/interconnect/common/state_updater.h>
#include "common.h"
#include "active_state.h"

namespace skyline::gpu::interconnect::maxwell3d {
    /* Vertex Buffer */
    void VertexBufferState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, vertexStream.format, vertexStream.location, vertexStreamLimit);
    }

    VertexBufferState::VertexBufferState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine, u32 index) : engine{manager, dirtyHandle, engine}, index{index} {}

    void VertexBufferState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask) {
        size_t size{engine->vertexStreamLimit - engine->vertexStream.location + 1};

        if (engine->vertexStream.format.enable && engine->vertexStream.location != 0 && size) {
            view.Update(ctx, engine->vertexStream.location, size);
            if (*view) {
                ctx.executor.AttachBuffer(*view);
                view->GetBuffer()->PopulateReadBarrier(vk::PipelineStageFlagBits::eVertexInput, srcStageMask, dstStageMask);

                if (megaBufferBinding = view->TryMegaBuffer(ctx.executor.cycle, ctx.gpu.megaBufferAllocator, ctx.executor.executionTag);
                    megaBufferBinding)
                    builder.SetVertexBuffer(index, megaBufferBinding, ctx.gpu.traits.supportsExtendedDynamicState, engine->vertexStream.format.stride);
                else
                    builder.SetVertexBuffer(index, *view, ctx.gpu.traits.supportsExtendedDynamicState, engine->vertexStream.format.stride);

                return;
            } else {
                Logger::Warn("Unmapped vertex buffer: 0x{:X}", engine->vertexStream.location);
            }
        }

        megaBufferBinding = {};
        if (ctx.gpu.traits.supportsNullDescriptor)
            builder.SetVertexBuffer(index, BufferBinding{}, ctx.gpu.traits.supportsExtendedDynamicState, engine->vertexStream.format.stride);
        else
            builder.SetVertexBuffer(index, {ctx.gpu.megaBufferAllocator.Allocate(ctx.executor.cycle, 0).buffer}, ctx.gpu.traits.supportsExtendedDynamicState, engine->vertexStream.format.stride);
    }

    bool VertexBufferState::Refresh(InterconnectContext &ctx, StateUpdateBuilder &builder, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask) {
        if (*view)
            view->GetBuffer()->PopulateReadBarrier(vk::PipelineStageFlagBits::eVertexInput, srcStageMask, dstStageMask);

        if (megaBufferBinding) {
            if (auto newMegaBufferBinding{view->TryMegaBuffer(ctx.executor.cycle, ctx.gpu.megaBufferAllocator, ctx.executor.executionTag)};
                newMegaBufferBinding != megaBufferBinding) {

                megaBufferBinding = newMegaBufferBinding;
                if (megaBufferBinding)
                    builder.SetVertexBuffer(index, megaBufferBinding, ctx.gpu.traits.supportsExtendedDynamicState, engine->vertexStream.format.stride);
                else
                    builder.SetVertexBuffer(index, *view, ctx.gpu.traits.supportsExtendedDynamicState, engine->vertexStream.format.stride);
            }
        }
        return false;
    }

    void VertexBufferState::PurgeCaches() {
        view.PurgeCaches();
        megaBufferBinding = {};
    }

    /* Index Buffer Helpers */
    static vk::DeviceSize GetIndexBufferSize(engine::IndexBuffer::IndexSize indexSize, u32 elementCount) {
        switch (indexSize) {
            case engine::IndexBuffer::IndexSize::OneByte:
                return elementCount * 1;
            case engine::IndexBuffer::IndexSize::TwoBytes:
                return elementCount * 2;
            case engine::IndexBuffer::IndexSize::FourBytes:
                return elementCount * 4;
            default:
                throw exception("Unsupported index size enum value: {}", static_cast<u32>(indexSize));
        }
    }

    static vk::IndexType ConvertIndexType(engine::IndexBuffer::IndexSize indexSize) {
        switch (indexSize) {
            case engine::IndexBuffer::IndexSize::OneByte:
                return vk::IndexType::eUint8EXT;
            case engine::IndexBuffer::IndexSize::TwoBytes:
                return vk::IndexType::eUint16;
            case engine::IndexBuffer::IndexSize::FourBytes:
                return vk::IndexType::eUint32;
            default:
                throw exception("Unsupported index size enum value: {}", static_cast<u32>(indexSize));
        }
    }

    static BufferBinding GenerateQuadConversionIndexBuffer(InterconnectContext &ctx, engine::IndexBuffer::IndexSize indexType, BufferView &view, u32 firstIndex, u32 elementCount) {
        auto viewSpan{view.GetReadOnlyBackingSpan(false /* We attach above so always false */, []() {
            // TODO: see Read()
            Logger::Error("Dirty index buffer reads for attached buffers are unimplemented");
        })};

        size_t indexSize{1U << static_cast<u32>(indexType)};
        vk::DeviceSize indexBufferSize{conversion::quads::GetRequiredBufferSize(elementCount, indexSize)};
        auto quadConversionAllocation{ctx.gpu.megaBufferAllocator.Allocate(ctx.executor.cycle, indexBufferSize)};

        conversion::quads::GenerateIndexedQuadConversionBuffer(quadConversionAllocation.region.data(), viewSpan.subspan(GetIndexBufferSize(indexType, firstIndex)).data(), elementCount, ConvertIndexType(indexType));

        return {quadConversionAllocation.buffer, quadConversionAllocation.offset, indexBufferSize};
    }

    /* Index Buffer */
    void IndexBufferState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, indexBuffer.indexSize, indexBuffer.address, indexBuffer.limit);
    }

    IndexBufferState::IndexBufferState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    void IndexBufferState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask, bool quadConversion, bool estimateSize, u32 firstIndex, u32 elementCount) {
        didEstimateSize = estimateSize;
        usedElementCount = elementCount;
        usedFirstIndex = firstIndex;
        usedQuadConversion = quadConversion;

        size_t size{[&] () {
            if (estimateSize)
                return engine->indexBuffer.address - engine->indexBuffer.limit + 1;
            else
                return GetIndexBufferSize(engine->indexBuffer.indexSize, firstIndex + elementCount);
        }()};

        view.Update(ctx, engine->indexBuffer.address, size, !estimateSize);
        if (!*view) {
            Logger::Warn("Unmapped index buffer: 0x{:X}", engine->indexBuffer.address);
            return;
        }

        ctx.executor.AttachBuffer(*view);
        view->GetBuffer()->PopulateReadBarrier(vk::PipelineStageFlagBits::eVertexInput, srcStageMask, dstStageMask);

        indexType = ConvertIndexType(engine->indexBuffer.indexSize);

        if (quadConversion)
            megaBufferBinding = GenerateQuadConversionIndexBuffer(ctx, engine->indexBuffer.indexSize, *view, firstIndex, elementCount);
        else
            megaBufferBinding = view->TryMegaBuffer(ctx.executor.cycle, ctx.gpu.megaBufferAllocator, ctx.executor.executionTag);

        if (megaBufferBinding)
            builder.SetIndexBuffer(megaBufferBinding, indexType);
        else
            builder.SetIndexBuffer(*view, indexType);
    }

    bool IndexBufferState::Refresh(InterconnectContext &ctx, StateUpdateBuilder &builder, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask, bool quadConversion, bool estimateSize, u32 firstIndex, u32 elementCount) {
        if (*view)
            view->GetBuffer()->PopulateReadBarrier(vk::PipelineStageFlagBits::eVertexInput, srcStageMask, dstStageMask);

        if (didEstimateSize != estimateSize || (elementCount + firstIndex > usedElementCount + usedFirstIndex) || quadConversion != usedQuadConversion)
            return true;

        // TODO: optimise this to use buffer sequencing to avoid needing to regenerate the quad buffer every time. We can't use as it is rn though because sequences aren't globally unique and may conflict after buffer recreation
        if (usedQuadConversion) {
            megaBufferBinding = GenerateQuadConversionIndexBuffer(ctx, engine->indexBuffer.indexSize, *view, firstIndex, elementCount);
            builder.SetIndexBuffer(megaBufferBinding, indexType);
        } else if (megaBufferBinding) {
            if (auto newMegaBufferBinding{view->TryMegaBuffer(ctx.executor.cycle, ctx.gpu.megaBufferAllocator, ctx.executor.executionTag)};
                newMegaBufferBinding != megaBufferBinding) {

                megaBufferBinding = newMegaBufferBinding;
                if (megaBufferBinding)
                    builder.SetIndexBuffer(megaBufferBinding, indexType);
                else
                    builder.SetIndexBuffer(*view, indexType);
            }
        }

        return false;
    }

    void IndexBufferState::PurgeCaches() {
        view.PurgeCaches();
        megaBufferBinding = {};
    }

    /* Transform Feedback Buffer */
    void TransformFeedbackBufferState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, streamOutBuffer.address, streamOutBuffer.loadWritePointerStartOffset, streamOutBuffer.size, streamOutEnable);
    }

    TransformFeedbackBufferState::TransformFeedbackBufferState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine, u32 index) : engine{manager, dirtyHandle, engine}, index{index} {}

    void TransformFeedbackBufferState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask) {
        if (engine->streamOutEnable) {
            if (engine->streamOutBuffer.size) {
                view.Update(ctx, engine->streamOutBuffer.address + engine->streamOutBuffer.loadWritePointerStartOffset, engine->streamOutBuffer.size);

                if (*view) {
                    ctx.executor.AttachBuffer(*view);

                    if (view->GetBuffer()->SequencedCpuBackingWritesBlocked()) {
                        srcStageMask |= vk::PipelineStageFlagBits::eAllCommands;
                        dstStageMask |=  vk::PipelineStageFlagBits::eTransformFeedbackEXT;
                    }

                    view->GetBuffer()->MarkGpuDirty(ctx.executor.usageTracker);
                    builder.SetTransformFeedbackBuffer(index, *view);
                    return;
                } else {
                    Logger::Warn("Unmapped transform feedback buffer: 0x{:X}", engine->streamOutBuffer.address);
                }
            }

            // Bind an empty buffer ourselves since Vulkan doesn't support passing a VK_NULL_HANDLE xfb buffer
            builder.SetTransformFeedbackBuffer(index, {ctx.gpu.megaBufferAllocator.Allocate(ctx.executor.cycle, 0).buffer});
        }
    }

    bool TransformFeedbackBufferState::Refresh(InterconnectContext &ctx, StateUpdateBuilder &builder, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask) {
        if (*view && view->GetBuffer()->SequencedCpuBackingWritesBlocked()) {
            srcStageMask |= vk::PipelineStageFlagBits::eAllCommands;
            dstStageMask |=  vk::PipelineStageFlagBits::eTransformFeedbackEXT;
        }

        return false;
    }

    void TransformFeedbackBufferState::PurgeCaches() {
        view.PurgeCaches();
    }

    /* Viewport */
    void ViewportState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle,
                     viewport0.offsetX, viewport0.offsetY, viewport0.scaleX, viewport0.scaleY, viewport0.swizzle,
                     viewportClip0,
                     viewport.offsetX, viewport.offsetY, viewport.scaleX, viewport.scaleY, viewport.swizzle,
                     viewportClip,
                     windowOrigin,
                     viewportScaleOffsetEnable,
                     surfaceClip);
    }

    ViewportState::ViewportState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine, u32 index) : engine{manager, dirtyHandle, engine}, index{index} {}

    static vk::Viewport ConvertViewport(const engine::Viewport &viewport, const engine::ViewportClip &viewportClip, const engine::WindowOrigin &windowOrigin, bool viewportScaleOffsetEnable) {
        vk::Viewport vkViewport{};

        vkViewport.x = viewport.offsetX - viewport.scaleX; // Counteract the addition of the half of the width (o_x) to the host translation
        vkViewport.width = viewport.scaleX * 2.0f; // Counteract the division of the width (p_x) by 2 for the host scale
        vkViewport.y = viewport.offsetY - viewport.scaleY; // Counteract the addition of the half of the height (p_y/2 is center) to the host translation (o_y)
        vkViewport.height = viewport.scaleY * 2.0f; // Counteract the division of the height (p_y) by 2 for the host scale

        using CoordinateSwizzle = engine::Viewport::CoordinateSwizzle;
        if (viewport.swizzle.x != CoordinateSwizzle::PosX &&
            viewport.swizzle.y != CoordinateSwizzle::PosY && viewport.swizzle.y != CoordinateSwizzle::NegY &&
            viewport.swizzle.z != CoordinateSwizzle::PosZ &&
            viewport.swizzle.w != CoordinateSwizzle::PosW)
            throw exception("Unsupported viewport swizzle: {}x{}x{}x{}", engine::Viewport::ToString(viewport.swizzle.x),
                            engine::Viewport::ToString(viewport.swizzle.y),
                            engine::Viewport::ToString(viewport.swizzle.z),
                            engine::Viewport::ToString(viewport.swizzle.w));

        // If swizzle and origin mode cancel out then do nothing, otherwise flip the viewport
        if ((viewport.swizzle.y == CoordinateSwizzle::NegY) != (windowOrigin.lowerLeft != 0)) {
            // Flip the viewport given that the viewport origin is lower left or the viewport Y has been flipped via a swizzle but not if both are active at the same time
            vkViewport.y += vkViewport.height;
            vkViewport.height = -vkViewport.height;
        }


        // Clamp since we don't yet use VK_EXT_unrestricted_depth_range
        vkViewport.minDepth = std::clamp(viewportClip.minZ, 0.0f, 1.0f);
        vkViewport.maxDepth = std::clamp(viewportClip.maxZ, 0.0f, 1.0f);

        if (viewport.scaleZ < 0.0f)
            std::swap(vkViewport.minDepth, vkViewport.maxDepth);

        return vkViewport;
    }

    void ViewportState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder) {
        if (index != 0 && !ctx.gpu.traits.supportsMultipleViewports)
            return;

        if (!engine->viewportScaleOffsetEnable) {
            builder.SetViewport(index, vk::Viewport{
                .x = static_cast<float>(engine->surfaceClip.horizontal.x),
                .y = static_cast<float>(engine->surfaceClip.vertical.y),
                .width = engine->surfaceClip.horizontal.width ? static_cast<float>(engine->surfaceClip.horizontal.width) : 1.0f,
                .height = engine->surfaceClip.vertical.height ? static_cast<float>(engine->surfaceClip.vertical.height) : 1.0f,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            });
        } else if (engine->viewport.scaleX == 0.0f || engine->viewport.scaleY == 0.0f) {
            builder.SetViewport(index, ConvertViewport(engine->viewport0, engine->viewportClip0, engine->windowOrigin, engine->viewportScaleOffsetEnable));
        } else {
            builder.SetViewport(index, ConvertViewport(engine->viewport, engine->viewportClip, engine->windowOrigin, engine->viewportScaleOffsetEnable));
        }
    }

    /* Scissor */
    void ScissorState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, scissor);
    }

    ScissorState::ScissorState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine, u32 index) : engine{manager, dirtyHandle, engine}, index{index} {}

    void ScissorState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder) {
        if (index != 0 && !ctx.gpu.traits.supportsMultipleViewports)
            return;

        builder.SetScissor(index, [&]() {
            if (engine->scissor.enable) {
                const auto &vertical{engine->scissor.vertical};
                const auto &horizontal{engine->scissor.horizontal};

                return vk::Rect2D{
                    .offset = {
                        .y = vertical.yMin,
                        .x = horizontal.xMin
                    },
                    .extent = {
                        .height = static_cast<uint32_t>(vertical.yMax - vertical.yMin),
                        .width = static_cast<uint32_t>(horizontal.xMax - horizontal.xMin)
                    }
                };
            } else {
                return vk::Rect2D{
                    .extent.height = std::numeric_limits<i32>::max(),
                    .extent.width = std::numeric_limits<i32>::max(),
                };
            }
        }());
    }

    /* Line Width */
    void LineWidthState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, lineWidth, lineWidthAliased, aliasedLineWidthEnable);
    }

    LineWidthState::LineWidthState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    void LineWidthState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder) {
        float width{engine->aliasedLineWidthEnable ? engine->lineWidthAliased : engine->lineWidth};
        if (width != 1.0f && !ctx.gpu.traits.supportsWideLines)
            Logger::Warn("Wide lines used on guest but unsupported on host!");
        else
            builder.SetLineWidth(width);
    }

    /* Depth Bias */
    void DepthBiasState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, depthBias, depthBiasClamp, slopeScaleDepthBias);
    }

    DepthBiasState::DepthBiasState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    void DepthBiasState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder) {
        builder.SetDepthBias(engine->depthBias / 2.0f, engine->depthBiasClamp, engine->slopeScaleDepthBias);
    }

    /* Blend Constants */
    void BlendConstantsState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, blendConsts);
    }

    BlendConstantsState::BlendConstantsState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    void BlendConstantsState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder) {
        builder.SetBlendConstants(engine->blendConsts);
    }

    /* Depth Bounds */
    void DepthBoundsState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, depthBoundsMin, depthBoundsMax);
    }

    DepthBoundsState::DepthBoundsState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    void DepthBoundsState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder) {
        builder.SetDepthBounds(std::clamp(engine->depthBoundsMin, 0.0f, 1.0f), std::clamp(engine->depthBoundsMax, 0.0f, 1.0f));
    }

    /* Stencil Values */
    void StencilValuesState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, twoSidedStencilTestEnable, stencilValues, backStencilValues);
    }

    StencilValuesState::StencilValuesState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    void StencilValuesState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder) {
        vk::StencilFaceFlags face{engine->twoSidedStencilTestEnable ? vk::StencilFaceFlagBits::eFront : vk::StencilFaceFlagBits::eFrontAndBack};
        builder.SetBaseStencilState(face, engine->stencilValues.funcRef, engine->stencilValues.funcMask, engine->stencilValues.mask);

        if (engine->twoSidedStencilTestEnable)
            builder.SetBaseStencilState(vk::StencilFaceFlagBits::eBack, engine->backStencilValues.funcRef, engine->backStencilValues.funcMask, engine->backStencilValues.mask);
    }

    ActiveState::ActiveState(DirtyManager &manager, const EngineRegisters &engineRegisters)
        : pipeline{manager, engineRegisters.pipelineRegisters},
          vertexBuffers{util::MergeInto<dirty::ManualDirtyState<VertexBufferState>, engine::VertexStreamCount>(manager, engineRegisters.vertexBuffersRegisters, util::IncrementingT<u32>{})},
          indexBuffer{manager, engineRegisters.indexBufferRegisters},
          transformFeedbackBuffers{util::MergeInto<dirty::ManualDirtyState<TransformFeedbackBufferState>, engine::StreamOutBufferCount>(manager, engineRegisters.transformFeedbackBuffersRegisters, util::IncrementingT<u32>{})},
          viewports{util::MergeInto<dirty::ManualDirtyState<ViewportState>, engine::ViewportCount>(manager, engineRegisters.viewportsRegisters, util::IncrementingT<u32>{})},
          scissors{util::MergeInto<dirty::ManualDirtyState<ScissorState>, engine::ViewportCount>(manager, engineRegisters.scissorsRegisters, util::IncrementingT<u32>{})},
          lineWidth{manager, engineRegisters.lineWidthRegisters},
          depthBias{manager, engineRegisters.depthBiasRegisters},
          blendConstants{manager, engineRegisters.blendConstantsRegisters},
          depthBounds{manager, engineRegisters.depthBoundsRegisters},
          stencilValues{manager, engineRegisters.stencilValuesRegisters},
          directState{pipeline.Get().directState} {}

    void ActiveState::MarkAllDirty() {
        auto dirtyFunc{[&](auto &stateElem) { stateElem.MarkDirty(true); }};

        dirtyFunc(pipeline);
        ranges::for_each(vertexBuffers, dirtyFunc);
        dirtyFunc(indexBuffer);
        ranges::for_each(transformFeedbackBuffers, dirtyFunc);
        ranges::for_each(viewports, dirtyFunc);
        ranges::for_each(scissors, dirtyFunc);
        dirtyFunc(lineWidth);
        dirtyFunc(depthBias);
        dirtyFunc(blendConstants);
        dirtyFunc(depthBounds);
        dirtyFunc(stencilValues);
    }

    void ActiveState::Update(InterconnectContext &ctx, Textures &textures, ConstantBufferSet &constantBuffers, StateUpdateBuilder &builder,
                             bool indexed, engine::DrawTopology topology, bool estimateIndexBufferSize, u32 drawFirstIndex, u32 drawElementCount,
                             vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask) {
        TRACE_EVENT("gpu", "ActiveState::Update");
        if (topology != directState.inputAssembly.GetPrimitiveTopology()) {
            directState.inputAssembly.SetPrimitiveTopology(topology);
            pipeline.MarkDirty(false);
        }

        auto updateFunc{[&](auto &stateElem, auto &&... args) { stateElem.Update(ctx, builder, args...); }};
        auto updateFuncBuffer{[&](auto &stateElem, auto &&... args) { stateElem.Update(ctx, builder, srcStageMask, dstStageMask, args...); }};

        pipeline.Update(ctx, textures, constantBuffers, builder);
        ranges::for_each(vertexBuffers, updateFuncBuffer);
        if (indexed)
            updateFuncBuffer(indexBuffer, directState.inputAssembly.NeedsQuadConversion(), estimateIndexBufferSize, drawFirstIndex, drawElementCount);
        ranges::for_each(transformFeedbackBuffers, updateFuncBuffer);
        ranges::for_each(viewports, updateFunc);
        ranges::for_each(scissors, updateFunc);
        updateFunc(lineWidth);
        updateFunc(depthBias);
        updateFunc(blendConstants);
        updateFunc(depthBounds);
        updateFunc(stencilValues);
    }

    Pipeline *ActiveState::GetPipeline() {
        return pipeline.Get().pipeline;
    }

    span<TextureView *> ActiveState::GetColorAttachments() {
        return pipeline.Get().colorAttachments;
    }

    TextureView *ActiveState::GetDepthAttachment() {
        return pipeline.Get().depthAttachment;
    }

    std::shared_ptr<TextureView> ActiveState::GetColorRenderTargetForClear(InterconnectContext &ctx, size_t index) {
        return pipeline.Get().GetColorRenderTargetForClear(ctx, index);
    }

    std::shared_ptr<TextureView> ActiveState::GetDepthRenderTargetForClear(InterconnectContext &ctx) {
        return pipeline.Get().GetDepthRenderTargetForClear(ctx);
    }
}
