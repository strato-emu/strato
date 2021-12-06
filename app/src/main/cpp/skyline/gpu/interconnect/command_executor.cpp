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

    void CommandExecutor::AttachTexture(const std::shared_ptr<Texture> &texture) {
        if (!syncTextures.contains(texture.get())) {
            texture->WaitOnFence();
            texture->cycle = cycle;
            cycle->AttachObject(texture);
            syncTextures.emplace(texture.get());
        }
    }

    void CommandExecutor::AttachBuffer(const std::shared_ptr<Buffer> &buffer) {
        if (!syncBuffers.contains(buffer.get())) {
            buffer->WaitOnFence();
            buffer->cycle = cycle;
            cycle->AttachObject(buffer);
            syncBuffers.emplace(buffer.get());
        }
    }

    void CommandExecutor::AddSubpass(const std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &)> &function, vk::Rect2D renderArea, span<TextureView *> inputAttachments, span<TextureView *> colorAttachments, TextureView *depthStencilAttachment) {
        for (const auto &attachments : {inputAttachments, colorAttachments})
            for (const auto &attachment : attachments)
                AttachTexture(attachment->texture);
        if (depthStencilAttachment)
            AttachTexture(depthStencilAttachment->texture);

        bool newRenderPass{CreateRenderPass(renderArea)};
        renderPass->AddSubpass(inputAttachments, colorAttachments, depthStencilAttachment ? &*depthStencilAttachment : nullptr);
        if (newRenderPass)
            nodes.emplace_back(std::in_place_type_t<node::FunctionNode>(), function);
        else
            nodes.emplace_back(std::in_place_type_t<node::NextSubpassFunctionNode>(), function);
    }

    void CommandExecutor::AddClearColorSubpass(TextureView *attachment, const vk::ClearColorValue &value) {
        AttachTexture(attachment->texture);

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
                nodes.emplace_back(std::in_place_type_t<node::FunctionNode>(), function);
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
                    texture->SynchronizeHostWithBuffer(commandBuffer, cycle);

                for (auto buffer : syncBuffers)
                    buffer->SynchronizeHostWithCycle(cycle);

                using namespace node;
                for (NodeVariant &node : nodes) {
                    #define NODE(name) [&](name& node) { node(commandBuffer, cycle, gpu); }
                    std::visit(VariantVisitor{
                        NODE(FunctionNode),
                        NODE(RenderPassNode),
                        NODE(NextSubpassNode),
                        NODE(NextSubpassFunctionNode),
                        NODE(RenderPassEndNode),
                    }, node);
                    #undef NODE
                }

                for (auto texture : syncTextures)
                    texture->SynchronizeGuestWithBuffer(commandBuffer, cycle);

                for (auto buffer : syncBuffers)
                    buffer->SynchronizeGuestWithCycle(cycle);

                commandBuffer.end();

                gpu.scheduler.SubmitCommandBuffer(commandBuffer, activeCommandBuffer.GetFence());
                cycle = activeCommandBuffer.Reset();
            }

            nodes.clear();
            syncTextures.clear();
        }
    }
}
