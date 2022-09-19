// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/Ryujinx/)
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu/interconnect/command_executor.h>
#include <vulkan/vulkan_structs.hpp>
#include "common/utils.h"
#include "maxwell_3d.h"
#include "common.h"
#include "state_updater.h"

namespace skyline::gpu::interconnect::maxwell3d {
    Maxwell3D::Maxwell3D(GPU &gpu,
                         soc::gm20b::ChannelContext &channelCtx,
                         gpu::interconnect::CommandExecutor &executor,
                         nce::NCE &nce,
                         skyline::kernel::MemoryManager &memoryManager,
                         DirtyManager &manager,
                         const EngineRegisterBundle &registerBundle)
        : ctx{channelCtx, executor, gpu, nce, memoryManager},
          activeState{manager, registerBundle.activeStateRegisters},
          clearEngineRegisters{registerBundle.clearRegisters},
          constantBuffers{manager, registerBundle.constantBufferSelectorRegisters},
          directState{activeState.directState} {
        executor.AddFlushCallback([this] {
            activeState.MarkAllDirty();
            constantBuffers.MarkAllDirty();
        });
    }

    vk::Rect2D Maxwell3D::GetClearScissor() {
        const auto &clearSurfaceControl{clearEngineRegisters.clearSurfaceControl};

        const auto &surfaceClip{clearEngineRegisters.surfaceClip};
        vk::Rect2D scissor{
            {surfaceClip.horizontal.x, surfaceClip.vertical.y},
            {surfaceClip.horizontal.width, surfaceClip.vertical.height}
        };

        auto rectIntersection{[](const vk::Rect2D &a, const vk::Rect2D &b) -> vk::Rect2D {
            vk::Offset2D maxOffset{std::max(a.offset.x, b.offset.x), std::max(a.offset.y, b.offset.y)};
            i32 signedWidth{std::min(a.offset.x + static_cast<i32>(a.extent.width), b.offset.x + static_cast<i32>(b.extent.width)) - maxOffset.x};
            i32 signedHeight{std::min(a.offset.y + static_cast<i32>(a.extent.height), b.offset.y + static_cast<i32>(b.extent.height)) - maxOffset.y};

            return {
                maxOffset,
                {static_cast<u32>(std::max(signedWidth, 0)),
                 static_cast<u32>(std::max(signedHeight, 0))}
            };
        }};

        if (clearSurfaceControl.useClearRect) {
            const auto &clearRect{clearEngineRegisters.clearRect};
            scissor = rectIntersection(scissor, {
                {clearRect.horizontal.xMin, clearRect.vertical.yMin},
                {static_cast<u32>(clearRect.horizontal.xMax - clearRect.horizontal.xMin), static_cast<u32>(clearRect.vertical.yMax - clearRect.vertical.yMin)}
            });
        }

        if (clearSurfaceControl.useScissor0) {
            const auto &scissor0{clearEngineRegisters.scissor0};
            scissor = rectIntersection(scissor, {
                {scissor0.horizontal.xMin, scissor0.vertical.yMin},
                {static_cast<u32>(scissor0.horizontal.xMax - scissor0.horizontal.xMin), static_cast<u32>(scissor0.vertical.yMax - scissor0.vertical.yMin)}
            });
        }

        if (clearSurfaceControl.useViewportClip0) {
            const auto &viewportClip0{clearEngineRegisters.viewportClip0};
            scissor = rectIntersection(scissor, {
                {viewportClip0.horizontal.x0, viewportClip0.vertical.y0},
                {viewportClip0.horizontal.width, viewportClip0.vertical.height}
            });
        }

        return scissor;
    }

    void Maxwell3D::LoadConstantBuffer(span<u32> data, u32 offset) {
        constantBuffers.Load(ctx, data, offset);
    }

    void Maxwell3D::BindConstantBuffer(engine::ShaderStage stage, u32 index, bool enable) {
        if (enable)
            constantBuffers.Bind(ctx, stage, index);
        else
            constantBuffers.Unbind(stage, index);
    }

    void Maxwell3D::DisableQuickConstantBufferBind() {
        constantBuffers.DisableQuickBind();
    }

    void Maxwell3D::Clear(engine::ClearSurface &clearSurface) {
        auto scissor{GetClearScissor()};
        if (scissor.extent.width == 0 || scissor.extent.height == 0)
            return;

        auto needsAttachmentClearCmd{[&](auto &view) {
            return scissor.offset.x != 0 || scissor.offset.y != 0 ||
                scissor.extent != vk::Extent2D{view->texture->dimensions} ||
                view->range.layerCount != 1 || view->range.baseArrayLayer != 0 || clearSurface.rtArrayIndex != 0;
        }};

        auto clearRects{util::MakeFilledArray<vk::ClearRect, 2>(vk::ClearRect{.rect = scissor, .baseArrayLayer = clearSurface.rtArrayIndex, .layerCount = 1})};
        boost::container::small_vector<vk::ClearAttachment, 2> clearAttachments;

        std::shared_ptr<TextureView> colorView{};
        std::shared_ptr<TextureView> depthStencilView{};

        if (clearSurface.rEnable || clearSurface.gEnable || clearSurface.bEnable || clearSurface.aEnable) {
            colorView = activeState.GetColorRenderTargetForClear(ctx, clearSurface.mrtSelect);
            ctx.executor.AttachTexture(&*colorView);

            if (!(clearSurface.rEnable && clearSurface.gEnable && clearSurface.bEnable && clearSurface.aEnable))
                Logger::Warn("Partial clears are unimplemented! Performing full clear instead");

            if (!(colorView->range.aspectMask & vk::ImageAspectFlagBits::eColor)) {
                Logger::Warn("Colour RT used in clear lacks colour aspect"); // TODO: Drop this check after texman rework
                return;
            }

            if (needsAttachmentClearCmd(colorView)) {
                clearAttachments.push_back({ .aspectMask = colorView->range.aspectMask, .clearValue = {clearEngineRegisters.colorClearValue} });
            } else {
                ctx.executor.AddClearColorSubpass(&*colorView, clearEngineRegisters.colorClearValue);
                colorView = {}; // Will avoid adding as colour RT
            }
        }

        if (clearSurface.stencilEnable || clearSurface.zEnable) {
            depthStencilView = activeState.GetDepthRenderTargetForClear(ctx);
            ctx.executor.AttachTexture(&*depthStencilView);

            bool viewHasDepth{depthStencilView->range.aspectMask & vk::ImageAspectFlagBits::eDepth}, viewHasStencil{depthStencilView->range.aspectMask & vk::ImageAspectFlagBits::eStencil};
            vk::ClearDepthStencilValue clearValue{
                .depth = clearEngineRegisters.depthClearValue,
                .stencil = clearEngineRegisters.stencilClearValue
            };


            if (!viewHasDepth && !viewHasStencil) {
                Logger::Warn("Depth stencil RT used in clear lacks depth or stencil aspects"); // TODO: Drop this check after texman rework
                return;
            }

            if (needsAttachmentClearCmd(depthStencilView) || (!clearSurface.stencilEnable && viewHasStencil) || (!clearSurface.zEnable && viewHasDepth )) { // Subpass clears write to all aspects of the texture, so we can't use them when only one component is enabled
                clearAttachments.push_back({ .aspectMask = depthStencilView->range.aspectMask, .clearValue = clearValue });
            } else {
                ctx.executor.AddClearDepthStencilSubpass(&*depthStencilView, clearValue);
                depthStencilView = {}; // Will avoid adding as depth-stencil RT
            }
        }

        // Always use surfaceClip for render area since it's more likely to match the renderArea of draws and avoid an RP break
        const auto &surfaceClip{clearEngineRegisters.surfaceClip};
        vk::Rect2D renderArea{{surfaceClip.horizontal.x, surfaceClip.vertical.y}, {surfaceClip.horizontal.width, surfaceClip.vertical.height}};

        std::array<TextureView *, 1> colorAttachments{colorView ? &*colorView : nullptr};
        ctx.executor.AddSubpass([clearAttachments, clearRects](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32) {
            commandBuffer.clearAttachments(clearAttachments, span(clearRects).first(clearAttachments.size()));
        }, renderArea, {}, colorView ? colorAttachments : span<TextureView *>{}, depthStencilView ? &*depthStencilView : nullptr);
    }

    void Maxwell3D::Draw(engine::DrawTopology topology, bool indexed, u32 count, u32 first, u32 instanceCount, u32 vertexOffset, u32 firstInstance) {
        StateUpdateBuilder builder{*ctx.executor.allocator};

        Pipeline *oldPipeline{activeState.GetPipeline()};
        activeState.Update(ctx, builder, indexed, topology, count);
        Pipeline *pipeline{activeState.GetPipeline()};

        if (((oldPipeline == pipeline) || (oldPipeline && oldPipeline->CheckBindingMatch(pipeline))) && constantBuffers.quickBindEnabled) {
            // If bindings between the old and new pipelines are the same we can reuse the descriptor sets given that quick bind is enabled (meaning that no buffer updates or calls to non-graphics engines have occurred that could invalidate them)
            if (constantBuffers.quickBind)
                // If only a single constant buffer has been rebound between draws we can perform a partial descriptor update
                pipeline->SyncDescriptorsQuickBind(ctx, constantBuffers.boundConstantBuffers, *constantBuffers.quickBind);
        } else {
            // If bindings have changed or quick bind is disabled, perform a full descriptor update
            pipeline->SyncDescriptors(ctx, constantBuffers.boundConstantBuffers);
        }

        const auto &surfaceClip{clearEngineRegisters.surfaceClip};
        vk::Rect2D scissor{
            {surfaceClip.horizontal.x, surfaceClip.vertical.y},
            {surfaceClip.horizontal.width, surfaceClip.vertical.height}
        };

        auto stateUpdater{builder.Build()};
        ctx.executor.AddSubpass([stateUpdater](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32) {
            stateUpdater.RecordAll(commandBuffer);
        }, scissor, {}, activeState.GetColorAttachments(), activeState.GetDepthAttachment());

        constantBuffers.ResetQuickBind();
    }
}