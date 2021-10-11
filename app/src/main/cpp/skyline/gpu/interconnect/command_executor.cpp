// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include "command_executor.h"

namespace skyline::gpu::interconnect {
    bool CommandExecutor::CreateRenderpass(vk::Rect2D renderArea) {
        if (renderpass && renderpass->renderArea != renderArea) {
            nodes.emplace_back(std::in_place_type_t<node::RenderpassEndNode>());
            renderpass = nullptr;
        }

        bool newRenderpass{renderpass == nullptr};
        if (newRenderpass)
            // We need to create a render pass if one doesn't already exist or the current one isn't compatible
            renderpass = &std::get<node::RenderpassNode>(nodes.emplace_back(std::in_place_type_t<node::RenderpassNode>(), renderArea));

        return newRenderpass;
    }

    void CommandExecutor::AddSubpass(const std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &)> &function, vk::Rect2D renderArea, std::vector<TextureView> inputAttachments, std::vector<TextureView> colorAttachments, std::optional<TextureView> depthStencilAttachment) {
        for (const auto &attachments : {inputAttachments, colorAttachments})
            for (const auto &attachment : attachments)
                syncTextures.emplace(attachment.backing.get());
        if (depthStencilAttachment)
            syncTextures.emplace(depthStencilAttachment->backing.get());

        bool newRenderpass{CreateRenderpass(renderArea)};
        renderpass->AddSubpass(inputAttachments, colorAttachments, depthStencilAttachment ? &*depthStencilAttachment : nullptr);
        if (newRenderpass)
            nodes.emplace_back(std::in_place_type_t<node::FunctionNode>(), function);
        else
            nodes.emplace_back(std::in_place_type_t<node::NextSubpassFunctionNode>(), function);
    }

    void CommandExecutor::AddClearColorSubpass(TextureView attachment, const vk::ClearColorValue &value) {
        bool newRenderpass{CreateRenderpass(vk::Rect2D{
            .extent = attachment.backing->dimensions,
        })};
        renderpass->AddSubpass({}, attachment, nullptr);

        if (renderpass->ClearColorAttachment(0, value)) {
            if (!newRenderpass)
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassNode>());
        } else {
            auto function{[scissor = attachment.backing->dimensions, value](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &) {
                commandBuffer.clearAttachments(vk::ClearAttachment{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .colorAttachment = 0,
                    .clearValue = value,
                }, vk::ClearRect{
                    .rect = scissor,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                });
            }};

            if (newRenderpass)
                nodes.emplace_back(std::in_place_type_t<node::FunctionNode>(), function);
            else
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassFunctionNode>(), function);
        }
    }

    void CommandExecutor::Execute() {
        if (!nodes.empty()) {
            TRACE_EVENT("gpu", "CommandExecutor::Execute");

            if (renderpass) {
                nodes.emplace_back(std::in_place_type_t<node::RenderpassEndNode>());
                renderpass = nullptr;
            }

            gpu.scheduler.SubmitWithCycle([this](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle) {
                for (auto texture : syncTextures)
                    texture->SynchronizeHostWithBuffer(commandBuffer, cycle);

                using namespace node;
                for (NodeVariant &node : nodes) {
                    #define NODE(name) [&](name& node) { node(commandBuffer, cycle, gpu); }
                    std::visit(VariantVisitor{
                        NODE(FunctionNode),
                        NODE(RenderpassNode),
                        NODE(NextSubpassNode),
                        NODE(NextSubpassFunctionNode),
                        NODE(RenderpassEndNode),
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
