// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/Ryujinx/)
// Copyright © 2022 yuzu Team and Contributors (https://github.com/yuzu-emu/)
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <range/v3/algorithm.hpp>
#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include <gpu/buffer_manager.h>
#include <gpu/interconnect/command_executor.h>
#include <gpu/interconnect/conversion/quads.h>
#include "common.h"
#include "active_state.h"

namespace skyline::gpu::interconnect::maxwell3d {
    /* General Buffer Helpers */
    static BufferBinding *AllocateMegaBufferBinding(InterconnectContext &ctx, BufferView &view) {
        auto megaBufferAllocation{view.AcquireMegaBuffer(ctx.executor.cycle, ctx.executor.AcquireMegaBufferAllocator())};
        if (megaBufferAllocation)
            return ctx.executor.allocator.EmplaceUntracked<BufferBinding>(megaBufferAllocation.buffer, megaBufferAllocation.offset, view.size);
        else
            return nullptr;
    }

    /* Vertex Buffer */
    void VertexBufferState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, vertexStream.format, vertexStream.location);
    }

    VertexBufferState::VertexBufferState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine, u32 index) : engine{manager, dirtyHandle, engine}, index{index} {}

    void VertexBufferState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder) {
        size_t size{engine->vertexStreamLimit - engine->vertexStream.location + 1};

        if (engine->vertexStream.format.enable && engine->vertexStream.location != 0) {
            view.Update(ctx, engine->vertexStream.location, size);
            ctx.executor.AttachBuffer(*view);

            if (megaBufferBinding = AllocateMegaBufferBinding(ctx, *view); megaBufferBinding)
                builder.SetVertexBuffer(index, megaBufferBinding);
            else
                builder.SetVertexBuffer(index, *view);
        } else {
            auto nullBinding{ctx.executor.allocator.EmplaceUntracked<BufferBinding>()};
            megaBufferBinding = {};
            builder.SetVertexBuffer(index, nullBinding);
        }
    }

    bool VertexBufferState::Refresh(InterconnectContext &ctx, StateUpdateBuilder &builder) {
        if (megaBufferBinding) {
            if (megaBufferBinding = AllocateMegaBufferBinding(ctx, *view); megaBufferBinding)
                builder.SetVertexBuffer(index, megaBufferBinding);
            else
                builder.SetVertexBuffer(index, *view);
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

    static BufferBinding *GenerateQuadConversionIndexBuffer(InterconnectContext &ctx, vk::IndexType indexType, BufferView &view, u32 elementCount) {
        auto viewSpan{view.GetReadOnlyBackingSpan(false /* We attach above so always false */, []() {
            // TODO: see Read()
            Logger::Error("Dirty index buffer reads for attached buffers are unimplemented");
        })};

        vk::DeviceSize indexBufferSize{conversion::quads::GetRequiredBufferSize(elementCount, indexType)};
        auto quadConversionAllocation{ctx.executor.AcquireMegaBufferAllocator().Allocate(ctx.executor.cycle, indexBufferSize)};

        conversion::quads::GenerateIndexedQuadConversionBuffer(quadConversionAllocation.region.data(), viewSpan.data(), elementCount, indexType);

        return ctx.executor.allocator.EmplaceUntracked<BufferBinding>(quadConversionAllocation.buffer, quadConversionAllocation.offset, indexBufferSize);
    }

    /* Index Buffer */
    void IndexBufferState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, indexBuffer.indexSize, indexBuffer.address);
    }

    IndexBufferState::IndexBufferState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    void IndexBufferState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder, bool quadConversion, u32 elementCount) {
        usedElementCount = elementCount;
        usedQuadConversion = quadConversion;

        size_t size{GetIndexBufferSize(engine->indexBuffer.indexSize, elementCount)};
        view.Update(ctx, engine->indexBuffer.address, size);
        ctx.executor.AttachBuffer(*view);

        indexType = ConvertIndexType(engine->indexBuffer.indexSize);

        if (quadConversion)
            megaBufferBinding = GenerateQuadConversionIndexBuffer(ctx, indexType, *view, elementCount);
        else
            megaBufferBinding = AllocateMegaBufferBinding(ctx, *view);

        if (megaBufferBinding)
            builder.SetIndexBuffer(megaBufferBinding, indexType);
        else
            builder.SetIndexBuffer(*view, indexType);
    }

    bool IndexBufferState::Refresh(InterconnectContext &ctx, StateUpdateBuilder &builder, bool quadConversion, u32 elementCount) {
        if (elementCount > usedElementCount)
            return true;

        if (quadConversion != usedQuadConversion)
            return true;

        // TODO: optimise this to use buffer sequencing to avoid needing to regenerate the quad buffer every time. We can't use as it is rn though because sequences aren't globally unique and may conflict after buffer recreation
        if (usedQuadConversion) {
            megaBufferBinding = GenerateQuadConversionIndexBuffer(ctx, indexType, *view, elementCount);
            builder.SetIndexBuffer(megaBufferBinding, indexType);
        } else if (megaBufferBinding) {
            if (megaBufferBinding = AllocateMegaBufferBinding(ctx, *view); megaBufferBinding)
                builder.SetIndexBuffer(megaBufferBinding, indexType);
            else
                builder.SetIndexBuffer(*view, indexType);
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

    void TransformFeedbackBufferState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder) {
        if (engine->streamOutEnable) {
            if (engine->streamOutBuffer.size) {
                view.Update(ctx, engine->streamOutBuffer.address + engine->streamOutBuffer.loadWritePointerStartOffset, engine->streamOutBuffer.size);
                ctx.executor.AttachBuffer(*view);

                //    view.GetBuffer()->MarkGpuDirty();
                builder.SetTransformFeedbackBuffer(index, *view);
            } else {
                // Bind an empty buffer ourselves since Vulkan doesn't support passing a VK_NULL_HANDLE xfb buffer
                 auto binding{ctx.executor.allocator.EmplaceUntracked<BufferBinding>(
                     ctx.executor.AcquireMegaBufferAllocator().Allocate(ctx.executor.cycle, 0).buffer)};
                builder.SetTransformFeedbackBuffer(index, binding);
            }
        }
    }

    void TransformFeedbackBufferState::PurgeCaches() {
        view.PurgeCaches();
    }

    /* Viewport */
    void ViewportState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, viewport.offsetX, viewport.offsetY, viewport.scaleX, viewport.scaleY, viewport.swizzle,
                     viewportClip,
                     windowOrigin,
                     viewportScaleOffsetEnable);
    }

    ViewportState::ViewportState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine, u32 index) : engine{manager, dirtyHandle, engine}, index{index} {}

    void ViewportState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder) {
        if (!engine->viewportScaleOffsetEnable)
            // https://github.com/Ryujinx/Ryujinx/pull/3328
            Logger::Warn("Viewport scale/offset disable is unimplemented");

        vk::Viewport viewport{};

        viewport.x = engine->viewport.offsetX - engine->viewport.scaleX; // Counteract the addition of the half of the width (o_x) to the host translation
        viewport.width = engine->viewport.scaleX * 2.0f; // Counteract the division of the width (p_x) by 2 for the host scale
        viewport.y = engine->viewport.offsetY - engine->viewport.scaleY; // Counteract the addition of the half of the height (p_y/2 is center) to the host translation (o_y)
        viewport.height = engine->viewport.scaleY * 2.0f; // Counteract the division of the height (p_y) by 2 for the host scale

        using CoordinateSwizzle = engine::Viewport::CoordinateSwizzle;
        if (engine->viewport.swizzle.x != CoordinateSwizzle::PosX &&
            engine->viewport.swizzle.y != CoordinateSwizzle::PosY && engine->viewport.swizzle.y != CoordinateSwizzle::NegY &&
            engine->viewport.swizzle.z != CoordinateSwizzle::PosZ &&
            engine->viewport.swizzle.w != CoordinateSwizzle::PosW)
            throw exception("Unsupported viewport swizzle: {}x{}x{}x{}", engine::Viewport::ToString(engine->viewport.swizzle.x),
                            engine::Viewport::ToString(engine->viewport.swizzle.y),
                            engine::Viewport::ToString(engine->viewport.swizzle.z),
                            engine::Viewport::ToString(engine->viewport.swizzle.w));

        // If swizzle and origin mode cancel out then do nothing, otherwise flip the viewport
        if ((engine->viewport.swizzle.y == CoordinateSwizzle::NegY) != (engine->windowOrigin.lowerLeft != 0)) {
            // Flip the viewport given that the viewport origin is lower left or the viewport Y has been flipped via a swizzle but not if both are active at the same time
            viewport.y += viewport.height;
            viewport.height = -viewport.height;
        }

        // Clamp since we don't yet use VK_EXT_unrestricted_depth_range
        viewport.minDepth = std::clamp(engine->viewportClip.minZ, 0.0f, 1.0f);
        viewport.maxDepth = std::clamp(engine->viewportClip.maxZ, 0.0f, 1.0f);
        builder.SetViewport(index, viewport);
    }

    /* Scissor */
    void ScissorState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, scissor);
    }

    ScissorState::ScissorState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine, u32 index) : engine{manager, dirtyHandle, engine}, index{index} {}

    void ScissorState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder) {
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
        builder.SetLineWidth(engine->aliasedLineWidthEnable ? engine->lineWidthAliased : engine->lineWidth);
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
        builder.SetDepthBounds(engine->depthBoundsMin, engine->depthBoundsMax);
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

    void ActiveState::Update(InterconnectContext &ctx, StateUpdateBuilder &builder, bool indexed, engine::DrawTopology topology, u32 drawElementCount) {
        if (topology != directState.inputAssembly.GetPrimitiveTopology()) {
            directState.inputAssembly.SetPrimitiveTopology(topology);
            pipeline.MarkDirty(false);
        }
        // TODO non-indexed quads

        auto updateFunc{[&](auto &stateElem, auto &&... args) { stateElem.Update(ctx, builder, args...); }};
        updateFunc(pipeline);
        ranges::for_each(vertexBuffers, updateFunc);
        if (indexed)
            updateFunc(indexBuffer, directState.inputAssembly.NeedsQuadConversion(), drawElementCount);
        ranges::for_each(transformFeedbackBuffers, updateFunc);
        ranges::for_each(viewports, updateFunc);
        ranges::for_each(scissors, updateFunc);
        updateFunc(lineWidth);
        updateFunc(depthBias);
        updateFunc(blendConstants);
        updateFunc(depthBounds);
        updateFunc(stencilValues);
    }

    std::shared_ptr<TextureView> ActiveState::GetColorRenderTargetForClear(InterconnectContext &ctx, size_t index) {
        return pipeline.Get().GetColorRenderTargetForClear(ctx, index);
    }

    std::shared_ptr<TextureView> ActiveState::GetDepthRenderTargetForClear(InterconnectContext &ctx) {
        return pipeline.Get().GetDepthRenderTargetForClear(ctx);
    }
}