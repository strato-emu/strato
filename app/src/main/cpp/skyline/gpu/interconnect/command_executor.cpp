// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include "command_executor.h"

namespace skyline::gpu::interconnect {
    void CommandExecutor::AddSubpass(const std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &)> &function, vk::Rect2D renderArea, std::vector<TextureView> inputAttachments, std::vector<TextureView> colorAttachments, std::optional<TextureView> depthStencilAttachment) {
        if (renderpass) { // TODO: Subpass support (&& renderpass->renderArea != renderArea)
            nodes.emplace_back(std::in_place_type_t<node::RenderpassEndNode>());
            renderpass = nullptr;
        }

        bool newRenderpass{renderpass == nullptr};
        if (newRenderpass)
            // We need to create a render pass if one doesn't already exist or the current one isn't compatible
            renderpass = &std::get<node::RenderpassNode>(nodes.emplace_back(std::in_place_type_t<node::RenderpassNode>(), renderArea));

        renderpass->AddSubpass(inputAttachments, colorAttachments, depthStencilAttachment);
        if (newRenderpass)
            nodes.emplace_back(std::in_place_type_t<node::FunctionNode>(), function);
        else
            nodes.emplace_back(std::in_place_type_t<node::NextSubpassNode>(), function);
    }

    void CommandExecutor::Execute() {
        if (!nodes.empty()) {
            if (renderpass) {
                nodes.emplace_back(std::in_place_type_t<node::RenderpassEndNode>());
                renderpass = nullptr;
            }

            gpu.scheduler.SubmitWithCycle([this](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle) {
                using namespace node;
                for (NodeVariant &node : nodes) {
                    std::visit(VariantVisitor{
                        [&](FunctionNode &node) { node(commandBuffer, cycle, gpu); },
                        [&](RenderpassNode &node) { node(commandBuffer, cycle, gpu); },
                        [&](RenderpassEndNode &node) { node(commandBuffer, cycle, gpu); },
                    }, node);
                }
            });

            nodes.clear();
        }
    }
}
