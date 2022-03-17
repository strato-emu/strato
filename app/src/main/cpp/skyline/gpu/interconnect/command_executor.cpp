// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include "command_executor.h"

namespace skyline::gpu::interconnect {
    CommandExecutor::CommandExecutor(const DeviceState &state) : gpu(*state.gpu), activeCommandBuffer(gpu.scheduler.AllocateCommandBuffer()), cycle(activeCommandBuffer.GetFenceCycle()) {}

    CommandExecutor::~CommandExecutor() {
        cycle->Cancel();
    }

    bool CommandExecutor::CreateRenderPass(vk::Rect2D renderArea) {
        if (renderPass && renderPass->renderArea != renderArea) {
            nodes.emplace_back(std::in_place_type_t<node::RenderPassEndNode>());
            renderPass = nullptr;
        }

        bool newRenderPass{renderPass == nullptr};
        if (newRenderPass)
            // We need to create a render pass if one doesn't already exist or the current one isn't compatible
            renderPass = &std::get<node::RenderPassNode>(nodes.emplace_back(std::in_place_type_t<node::RenderPassNode>(), renderArea));

        return newRenderPass;
    }

    void CommandExecutor::AttachTexture(TextureView *view) {
        auto texture{view->texture.get()};
        if (!syncTextures.contains(texture)) {
            texture->WaitOnFence();
            texture->cycle = cycle;
            syncTextures.emplace(texture);
        }
        cycle->AttachObject(view->shared_from_this());
    }

    void CommandExecutor::AttachBuffer(BufferView view) {
        auto buffer{view.buffer.get()};
        if (!syncBuffers.contains(buffer)) {
            buffer->WaitOnFence();
            buffer->cycle = cycle;
            cycle->AttachObject(view);
            syncBuffers.emplace(buffer);
        }
    }

    void CommandExecutor::AttachDependency(const std::shared_ptr<FenceCycleDependency> &dependency) {
        cycle->AttachObject(dependency);
    }

    void CommandExecutor::AddSubpass(std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32)> &&function, vk::Rect2D renderArea, span<TextureView *> inputAttachments, span<TextureView *> colorAttachments, TextureView *depthStencilAttachment) {
        bool newRenderPass{CreateRenderPass(renderArea)};
        renderPass->AddSubpass(inputAttachments, colorAttachments, depthStencilAttachment ? &*depthStencilAttachment : nullptr);
        if (newRenderPass)
            nodes.emplace_back(std::in_place_type_t<node::SubpassFunctionNode>(), std::forward<std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32)>>(function));
        else
            nodes.emplace_back(std::in_place_type_t<node::NextSubpassFunctionNode>(), std::forward<std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32)>>(function));
    }

    void CommandExecutor::AddClearColorSubpass(TextureView *attachment, const vk::ClearColorValue &value) {
        bool newRenderPass{CreateRenderPass(vk::Rect2D{
            .extent = attachment->texture->dimensions,
        })};
        renderPass->AddSubpass({}, attachment, nullptr);

        if (renderPass->ClearColorAttachment(0, value)) {
            if (!newRenderPass)
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassNode>());
        } else {
            auto function{[scissor = attachment->texture->dimensions, value](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32) {
                commandBuffer.clearAttachments(vk::ClearAttachment{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .colorAttachment = 0,
                    .clearValue = value,
                }, vk::ClearRect{
                    .rect = vk::Rect2D{.extent = scissor},
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                });
            }};

            if (newRenderPass)
                nodes.emplace_back(std::in_place_type_t<node::SubpassFunctionNode>(), function);
            else
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassFunctionNode>(), function);
        }
    }

    void CommandExecutor::AddClearDepthStencilSubpass(TextureView *attachment, const vk::ClearDepthStencilValue &value) {
        bool newRenderPass{CreateRenderPass(vk::Rect2D{
            .extent = attachment->texture->dimensions,
        })};
        renderPass->AddSubpass({}, {}, attachment);

        if (renderPass->ClearDepthStencilAttachment(value)) {
            if (!newRenderPass)
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassNode>());
        } else {
            auto function{[aspect = attachment->format->vkAspect, extent = attachment->texture->dimensions, value](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32) {
                commandBuffer.clearAttachments(vk::ClearAttachment{
                    .aspectMask = aspect,
                    .clearValue = value,
                }, vk::ClearRect{
                    .rect.extent = extent,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                });
            }};

            if (newRenderPass)
                nodes.emplace_back(std::in_place_type_t<node::SubpassFunctionNode>(), function);
            else
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassFunctionNode>(), function);
        }
    }

    void CommandExecutor::Execute() {
        if (!nodes.empty()) {
            TRACE_EVENT("gpu", "CommandExecutor::Execute");

            if (renderPass) {
                nodes.emplace_back(std::in_place_type_t<node::RenderPassEndNode>());
                renderPass = nullptr;
            }

            {
                auto &commandBuffer{*activeCommandBuffer};
                commandBuffer.begin(vk::CommandBufferBeginInfo{
                    .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                });

                for (auto texture : syncTextures)
                    texture->SynchronizeHostWithBuffer(commandBuffer, cycle, true);

                for (auto buffer : syncBuffers)
                    buffer->SynchronizeHostWithCycle(cycle, true);

                vk::RenderPass lRenderPass;
                u32 subpassIndex;

                using namespace node;
                for (NodeVariant &node : nodes) {
                    #define NODE(name) [&](name& node) { node(commandBuffer, cycle, gpu); }
                    std::visit(VariantVisitor{
                        NODE(FunctionNode),

                        [&](RenderPassNode &node) {
                            lRenderPass = node(commandBuffer, cycle, gpu);
                            subpassIndex = 0;
                        },

                        [&](NextSubpassNode &node) {
                            node(commandBuffer, cycle, gpu);
                            ++subpassIndex;
                        },
                        [&](SubpassFunctionNode &node) { node(commandBuffer, cycle, gpu, lRenderPass, subpassIndex); },
                        [&](NextSubpassFunctionNode &node) { node(commandBuffer, cycle, gpu, lRenderPass, ++subpassIndex); },

                        NODE(RenderPassEndNode),
                    }, node);
                    #undef NODE
                }

                commandBuffer.end();
                gpu.scheduler.SubmitCommandBuffer(commandBuffer, activeCommandBuffer.GetFence());

                nodes.clear();
                syncTextures.clear();

                cycle = activeCommandBuffer.Reset();
            }
        }
    }
}
