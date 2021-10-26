// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include "command_executor.h"

namespace skyline::gpu::interconnect {
    CommandExecutor::CommandExecutor(const DeviceState &state) : gpu(*state.gpu) {}

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

    void CommandExecutor::AddSubpass(const std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &)> &function, vk::Rect2D renderArea, std::vector<TextureView> inputAttachments, std::vector<TextureView> colorAttachments, std::optional<TextureView> depthStencilAttachment) {
        for (const auto &attachments : {inputAttachments, colorAttachments})
            for (const auto &attachment : attachments)
                syncTextures.emplace(attachment.backing.get());
        if (depthStencilAttachment)
            syncTextures.emplace(depthStencilAttachment->backing.get());

        bool newRenderPass{CreateRenderPass(renderArea)};
        renderPass->AddSubpass(inputAttachments, colorAttachments, depthStencilAttachment ? &*depthStencilAttachment : nullptr);
        if (newRenderPass)
            nodes.emplace_back(std::in_place_type_t<node::FunctionNode>(), function);
        else
            nodes.emplace_back(std::in_place_type_t<node::NextSubpassFunctionNode>(), function);
    }

    void CommandExecutor::AddClearColorSubpass(TextureView attachment, const vk::ClearColorValue &value) {
        bool newRenderPass{CreateRenderPass(vk::Rect2D{
            .extent = attachment.backing->dimensions,
        })};
        renderPass->AddSubpass({}, attachment, nullptr);

        if (renderPass->ClearColorAttachment(0, value)) {
            if (!newRenderPass)
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassNode>());
        } else {
            auto function{[scissor = attachment.backing->dimensions, value](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &) {
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

            gpu.scheduler.SubmitWithCycle([this](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle) {
                for (auto texture : syncTextures)
                    texture->SynchronizeHostWithBuffer(commandBuffer, cycle);

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
            })->Wait();

            nodes.clear();
            syncTextures.clear();
        }
    }
}
