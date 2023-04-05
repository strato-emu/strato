// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/Ryujinx/)
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu/interconnect/command_executor.h>
#include <gpu/interconnect/conversion/quads.h>
#include <gpu/interconnect/common/state_updater.h>
#include <soc/gm20b/channel.h>
#include "common/utils.h"
#include "maxwell_3d.h"
#include "common.h"

namespace skyline::gpu::interconnect::maxwell3d {
    Maxwell3D::Maxwell3D(GPU &gpu,
                         soc::gm20b::ChannelContext &channelCtx,
                         nce::NCE &nce,
                         skyline::kernel::MemoryManager &memoryManager,
                         DirtyManager &manager,
                         const EngineRegisterBundle &registerBundle)
        : ctx{channelCtx, channelCtx.executor, gpu, nce, memoryManager},
          activeState{manager, registerBundle.activeStateRegisters},
          clearEngineRegisters{registerBundle.clearRegisters},
          constantBuffers{manager, registerBundle.constantBufferSelectorRegisters},
          samplers{manager, registerBundle.samplerPoolRegisters},
          samplerBinding{registerBundle.samplerBinding},
          textures{manager, registerBundle.texturePoolRegisters},
          directState{activeState.directState},
          queries{gpu} {
        ctx.executor.AddFlushCallback([this] {
            if (attachedDescriptorSets) {
                ctx.executor.AttachDependency(attachedDescriptorSets);
                attachedDescriptorSets = nullptr;
                activeDescriptorSet = nullptr;
            }

            activeState.MarkAllDirty();
            constantBuffers.MarkAllDirty();
            samplers.MarkAllDirty();
            textures.MarkAllDirty();
            quadConversionBufferAttached = false;
            constantBuffers.DisableQuickBind();
            queries.PurgeCaches(ctx);
        });

        ctx.executor.AddPipelineChangeCallback([this] {
            activeState.MarkAllDirty();
            activeDescriptorSet = nullptr;
        });
    }

    vk::DeviceSize Maxwell3D::UpdateQuadConversionBuffer(u32 count, u32 firstVertex) {
        vk::DeviceSize offset{conversion::quads::GetRequiredBufferSize(firstVertex, sizeof(u32))};
        vk::DeviceSize size{conversion::quads::GetRequiredBufferSize(count, sizeof(u32)) + offset};

        if (!quadConversionBuffer || quadConversionBuffer->size_bytes() < size) {
            quadConversionBuffer = std::make_shared<memory::Buffer>(ctx.gpu.memory.AllocateBuffer(util::AlignUp(size, PAGE_SIZE)));
            conversion::quads::GenerateQuadListConversionBuffer(quadConversionBuffer->cast<u32>().data(), firstVertex + count);
            quadConversionBufferAttached = false;
        }

        if (!quadConversionBufferAttached) {
            ctx.executor.AttachDependency(quadConversionBuffer);
            quadConversionBufferAttached = true;
        }

        return offset;
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

        if (clearSurfaceControl.useScissor0 && clearEngineRegisters.scissor0.enable) {
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

    vk::Rect2D Maxwell3D::GetDrawScissor() {
        const auto &surfaceClip{clearEngineRegisters.surfaceClip};
        vk::Rect2D scissor{{surfaceClip.horizontal.x, surfaceClip.vertical.y},
                           {surfaceClip.horizontal.width, surfaceClip.vertical.height}};

        auto colorAttachments{activeState.GetColorAttachments()};
        auto depthStencilAttachment{activeState.GetDepthAttachment()};
        auto depthStencilAttachmentSpan{depthStencilAttachment ? span<TextureView *>(depthStencilAttachment) : span<TextureView *>()};
        for (auto attachment : ranges::views::concat(colorAttachments, depthStencilAttachmentSpan)) {
            if (attachment) {
                scissor.extent.width = std::min(scissor.extent.width, static_cast<u32>(static_cast<i32>(attachment->texture->dimensions.width) - scissor.offset.x));
                scissor.extent.height = std::min(scissor.extent.height, static_cast<u32>(static_cast<i32>(attachment->texture->dimensions.height) - scissor.offset.y));
            }
        }

        return scissor;
    }

     void Maxwell3D::PrepareDraw(StateUpdateBuilder &builder,
                                 engine::DrawTopology topology, bool indexed, bool estimateIndexBufferSize, u32 firstIndex, u32 count,
                                 vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask) {
         Pipeline *oldPipeline{activeState.GetPipeline()};
         samplers.Update(ctx, samplerBinding.value == engine::SamplerBinding::Value::ViaHeaderBinding);
         activeState.Update(ctx, textures, constantBuffers.boundConstantBuffers,
                            builder,
                            indexed, topology, estimateIndexBufferSize, firstIndex, count,
                            srcStageMask, dstStageMask);
         Pipeline *pipeline{activeState.GetPipeline()};
         activeDescriptorSetSampledImages.resize(pipeline->GetTotalSampledImageCount());


         auto *descUpdateInfo{[&]() -> DescriptorUpdateInfo * {
             if (((oldPipeline == pipeline) || (oldPipeline && oldPipeline->CheckBindingMatch(pipeline))) && constantBuffers.quickBindEnabled) {
                 // If bindings between the old and new pipelines are the same we can reuse the descriptor sets given that quick bind is enabled (meaning that no buffer updates or calls to non-graphics engines have occurred that could invalidate them)
                 if (constantBuffers.quickBind)
                     // If only a single constant buffer has been rebound between draws we can perform a partial descriptor update
                     return pipeline->SyncDescriptorsQuickBind(ctx, constantBuffers.boundConstantBuffers, samplers, textures,
                                                               *constantBuffers.quickBind, activeDescriptorSetSampledImages,
                                                               srcStageMask, dstStageMask);
                 else
                     return nullptr;
             } else {
                 // If bindings have changed or quick bind is disabled, perform a full descriptor update
                 return pipeline->SyncDescriptors(ctx, constantBuffers.boundConstantBuffers, samplers, textures,
                                                  activeDescriptorSetSampledImages,
                                                  srcStageMask, dstStageMask);
             }
         }()};

         if (oldPipeline != pipeline)
             // If the pipeline has changed, we need to update the pipeline state
             builder.SetPipeline(pipeline->compiledPipeline.pipeline, vk::PipelineBindPoint::eGraphics);

         if (descUpdateInfo) {
             if (ctx.gpu.traits.supportsPushDescriptors) {
                 builder.SetDescriptorSetWithPush(descUpdateInfo);
             } else {
                 if (!attachedDescriptorSets)
                     attachedDescriptorSets = std::make_shared<boost::container::static_vector<DescriptorAllocator::ActiveDescriptorSet, DescriptorBatchSize>>();

                 auto newSet{&attachedDescriptorSets->emplace_back(ctx.gpu.descriptor.AllocateSet(descUpdateInfo->descriptorSetLayout))};
                 auto *oldSet{activeDescriptorSet};
                 activeDescriptorSet = newSet;

                 builder.SetDescriptorSetWithUpdate(descUpdateInfo, activeDescriptorSet, oldSet);

                 if (attachedDescriptorSets->size() == DescriptorBatchSize) {
                     ctx.executor.AttachDependency(attachedDescriptorSets);
                     attachedDescriptorSets.reset();
                 }
             }
         }
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

        TRACE_EVENT("gpu", "Maxwell3D::Clear");
        ctx.executor.AddCheckpoint("Before clear");

        auto needsAttachmentClearCmd{[&](auto &view) {
            return scissor.offset.x != 0 || scissor.offset.y != 0 ||
                scissor.extent != vk::Extent2D{view->texture->dimensions} ||
                view->range.layerCount != 1 || view->range.baseArrayLayer != 0 || clearSurface.rtArrayIndex != 0;
        }};

        // Always use surfaceClip for render area since it's more likely to match the renderArea of draws and avoid an RP break
        const auto &surfaceClip{clearEngineRegisters.surfaceClip};
        vk::Rect2D renderArea{{surfaceClip.horizontal.x, surfaceClip.vertical.y}, {surfaceClip.horizontal.width, surfaceClip.vertical.height}};

        auto clearRects{util::MakeFilledArray<vk::ClearRect, 2>(vk::ClearRect{.rect = scissor, .baseArrayLayer = clearSurface.rtArrayIndex, .layerCount = 1})};
        boost::container::small_vector<vk::ClearAttachment, 2> clearAttachments;

        std::shared_ptr<TextureView> colorView{};
        std::shared_ptr<TextureView> depthStencilView{};

        if (clearSurface.rEnable || clearSurface.gEnable || clearSurface.bEnable || clearSurface.aEnable) {
            if (auto view{activeState.GetColorRenderTargetForClear(ctx, clearSurface.mrtSelect)}) {
                ctx.executor.AttachTexture(&*view);

                bool partialClear{!(clearSurface.rEnable && clearSurface.gEnable && clearSurface.bEnable && clearSurface.aEnable)};
                if (!(view->range.aspectMask & vk::ImageAspectFlagBits::eColor))
                    Logger::Warn("Colour RT used in clear lacks colour aspect"); // TODO: Drop this check after texman rework


                if (partialClear) {
                    ctx.gpu.helperShaders.clearHelperShader.Clear(ctx.gpu, view->range.aspectMask,
                                                                  (clearSurface.rEnable ? vk::ColorComponentFlagBits::eR : vk::ColorComponentFlags{}) |
                                                                  (clearSurface.gEnable ? vk::ColorComponentFlagBits::eG : vk::ColorComponentFlags{}) |
                                                                  (clearSurface.bEnable ? vk::ColorComponentFlagBits::eB : vk::ColorComponentFlags{}) |
                                                                  (clearSurface.aEnable ? vk::ColorComponentFlagBits::eA : vk::ColorComponentFlags{}),
                                                                  {clearEngineRegisters.colorClearValue}, &*view, [=](auto &&executionCallback) {
                        auto dst{view.get()};
                        ctx.executor.AddSubpass(std::move(executionCallback), renderArea, {}, {}, span<TextureView *>{dst}, nullptr);
                    });
                    ctx.executor.NotifyPipelineChange();
                } else if (needsAttachmentClearCmd(view)) {
                    clearAttachments.push_back({.aspectMask = view->range.aspectMask, .clearValue = {clearEngineRegisters.colorClearValue}});
                    colorView = view;
                } else {
                    ctx.executor.AddClearColorSubpass(&*view, clearEngineRegisters.colorClearValue);
                }
            }
        }

        if (clearSurface.stencilEnable || clearSurface.zEnable) {
            if (auto view{activeState.GetDepthRenderTargetForClear(ctx)}) {
                ctx.executor.AttachTexture(&*view);

                bool viewHasDepth{view->range.aspectMask & vk::ImageAspectFlagBits::eDepth}, viewHasStencil{view->range.aspectMask & vk::ImageAspectFlagBits::eStencil};
                vk::ImageAspectFlags clearAspectMask{(clearSurface.zEnable ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlags{}) |
                                                     (clearSurface.stencilEnable ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlags{})};
                clearAspectMask &= view->range.aspectMask;

                vk::ClearDepthStencilValue clearValue{
                    .depth = clearEngineRegisters.depthClearValue,
                    .stencil = clearEngineRegisters.stencilClearValue
                };

                if (!clearAspectMask) {
                    Logger::Warn("Depth stencil RT used in clear lacks depth or stencil aspects"); // TODO: Drop this check after texman rework
                    return;
                }

                if (needsAttachmentClearCmd(view) || (clearAspectMask != view->range.aspectMask)) { // Subpass clears write to all aspects of the texture, so we can't use them when only one component is enabled
                    clearAttachments.push_back({.aspectMask = clearAspectMask, .clearValue = clearValue});
                    depthStencilView = view;
                } else {
                    ctx.executor.AddClearDepthStencilSubpass(&*view, clearValue);
                }
            }
        }

        if (!clearAttachments.empty()) {
            std::array<TextureView *, 1> colorAttachments{colorView ? &*colorView : nullptr};
            ctx.executor.AddSubpass([clearAttachments, clearRects](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32) {
                commandBuffer.clearAttachments(clearAttachments, span(clearRects).first(clearAttachments.size()));
            }, renderArea, {}, {}, colorView ? colorAttachments : span<TextureView *>{}, depthStencilView ? &*depthStencilView : nullptr);
        }

        ctx.executor.AddCheckpoint("After clear");
    }

    void Maxwell3D::Draw(engine::DrawTopology topology, bool transformFeedbackEnable, bool indexed, u32 count, u32 first, u32 instanceCount, u32 vertexOffset, u32 firstInstance) {
        TRACE_EVENT("gpu", "Draw", "indexed", indexed, "count", count, "instanceCount", instanceCount);

        StateUpdateBuilder builder{*ctx.executor.allocator};
        vk::PipelineStageFlags srcStageMask{}, dstStageMask{};

        PrepareDraw(builder, topology, indexed, false, first, count, srcStageMask, dstStageMask);

        if (directState.inputAssembly.NeedsQuadConversion()) {
            count = conversion::quads::GetIndexCount(count);
            first = 0;

            if (!indexed) {
                // Use an index buffer to emulate quad lists with a triangle list input topology
                vk::DeviceSize offset{UpdateQuadConversionBuffer(count, first)};
                builder.SetIndexBuffer(BufferBinding{quadConversionBuffer->vkBuffer, offset}, vk::IndexType::eUint32);
                indexed = true;
            }
        }

        auto stateUpdater{builder.Build()};

        /**
         * @brief Struct that can be linearly allocated, holding all state for the draw to avoid a dynamic allocation with lambda captures
         */
        struct DrawParams {
            StateUpdater stateUpdater;
            u32 count;
            u32 first;
            u32 instanceCount;
            u32 vertexOffset;
            u32 firstInstance;
            bool indexed;
            bool transformFeedbackEnable;
        };
        auto *drawParams{ctx.executor.allocator->EmplaceUntracked<DrawParams>(DrawParams{stateUpdater,
                                                                                         count, first, instanceCount, vertexOffset, firstInstance, indexed,
                                                                                         ctx.gpu.traits.supportsTransformFeedback ? transformFeedbackEnable : false})};

        vk::Rect2D scissor{GetDrawScissor()};


        constantBuffers.ResetQuickBind();
        ctx.executor.AddCheckpoint("Before draw");
        ctx.executor.AddSubpass([drawParams](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &gpu, vk::RenderPass, u32) {
            drawParams->stateUpdater.RecordAll(gpu, commandBuffer);

            if (drawParams->transformFeedbackEnable)
                commandBuffer.beginTransformFeedbackEXT(0, {}, {});

            if (drawParams->indexed)
                commandBuffer.drawIndexed(drawParams->count, drawParams->instanceCount, drawParams->first, static_cast<i32>(drawParams->vertexOffset), drawParams->firstInstance);
            else
                commandBuffer.draw(drawParams->count, drawParams->instanceCount, drawParams->first, drawParams->firstInstance);

            if (drawParams->transformFeedbackEnable)
                commandBuffer.endTransformFeedbackEXT(0, {}, {});
        }, scissor, activeDescriptorSetSampledImages, {}, activeState.GetColorAttachments(), activeState.GetDepthAttachment(), !ctx.gpu.traits.quirks.relaxedRenderPassCompatibility, srcStageMask, dstStageMask);
        ctx.executor.AddCheckpoint("After draw");
    }

    void Maxwell3D::DrawIndirect(engine::DrawTopology topology, bool transformFeedbackEnable, bool indexed, span<u8> indirectBuffer, u32 count, u32 stride) {
        if (!count)
            return;

        TRACE_EVENT("gpu", "Indirect Draw", "buffer", reinterpret_cast<uintptr_t>(indirectBuffer.data()));

        StateUpdateBuilder builder{*ctx.executor.allocator};
        vk::PipelineStageFlags srcStageMask{}, dstStageMask{};

        PrepareDraw(builder, topology, indexed, true, 0, 0, srcStageMask, dstStageMask);

        if (directState.inputAssembly.NeedsQuadConversion())
            throw exception("Quad conversion is not supported for indirect draws!");

        if (indirectBufferView)
            indirectBufferView = indirectBufferView.GetBuffer()->TryGetView(indirectBuffer);
        if (!indirectBufferView)
            indirectBufferView = ctx.gpu.buffer.FindOrCreate(indirectBuffer, ctx.executor.tag, [this](std::shared_ptr<Buffer> buffer, ContextLock<Buffer> &&lock) {
                ctx.executor.AttachLockedBuffer(buffer, std::move(lock));
            });

        indirectBufferView.GetBuffer()->BlockSequencedCpuBackingWrites();

        auto stateUpdater{builder.Build()};

        /**
         * @brief Struct that can be linearly allocated, holding all state for the draw to avoid a dynamic allocation with lambda captures
         */
        struct DrawParams {
            StateUpdater stateUpdater;
            BufferView indirectBuffer;
            u32 count;
            u32 stride;
            bool indexed;
            bool transformFeedbackEnable;
        };
        auto *drawParams{ctx.executor.allocator->EmplaceUntracked<DrawParams>(DrawParams{stateUpdater,
                                                                                         indirectBufferView,
                                                                                         count, stride, indexed,
                                                                                         ctx.gpu.traits.supportsTransformFeedback ? transformFeedbackEnable : false})};

        auto scissor{GetDrawScissor()};
        constantBuffers.ResetQuickBind();

        ctx.executor.AddCheckpoint("Before indirect draw");
        ctx.executor.AddSubpass([drawParams](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &gpu, vk::RenderPass, u32) {
            drawParams->stateUpdater.RecordAll(gpu, commandBuffer);

            if (drawParams->transformFeedbackEnable)
                commandBuffer.beginTransformFeedbackEXT(0, {}, {});

            auto indirectBinding{drawParams->indirectBuffer.GetBinding(gpu)};
            if (drawParams->indexed)
                commandBuffer.drawIndexedIndirect(indirectBinding.buffer, indirectBinding.offset, drawParams->count, drawParams->stride);
            else
                commandBuffer.drawIndirect(indirectBinding.buffer,  indirectBinding.offset, drawParams->count, drawParams->stride);

            if (drawParams->transformFeedbackEnable)
                commandBuffer.endTransformFeedbackEXT(0, {}, {});
        }, scissor, activeDescriptorSetSampledImages, {}, activeState.GetColorAttachments(), activeState.GetDepthAttachment(), !ctx.gpu.traits.quirks.relaxedRenderPassCompatibility, srcStageMask, dstStageMask);
        ctx.executor.AddCheckpoint("After indirect draw");
    }

    void Maxwell3D::Query(soc::gm20b::IOVA address, engine::SemaphoreInfo::CounterType type, std::optional<u64> timestamp) {
        if (type != engine::SemaphoreInfo::CounterType::SamplesPassed) {
            Logger::Error("Unsupported query type: {}", static_cast<u32>(type));
            return;
        }

        queries.Query(ctx, address, Queries::CounterType::Occulusion, timestamp);
    }

    void Maxwell3D::ResetCounter(engine::ClearReportValue::Type type) {
        if (type != engine::ClearReportValue::Type::ZPassPixelCount) {
            Logger::Debug("Unsupported query type: {}", static_cast<u32>(type));
            return;
        }

        queries.ResetCounter(ctx, Queries::CounterType::Occulusion);
    }

    bool Maxwell3D::QueryPresentAtAddress(soc::gm20b::IOVA address) {
        return queries.QueryPresentAtAddress(address);
    }
}