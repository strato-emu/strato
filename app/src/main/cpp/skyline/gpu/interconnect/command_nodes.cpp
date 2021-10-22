// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "command_nodes.h"

namespace skyline::gpu::interconnect::node {
    RenderpassNode::Storage::~Storage() {
        if (device) {
            if (framebuffer)
                (**device).destroy(framebuffer, nullptr, *device->getDispatcher());
            if (renderpass)
                (**device).destroy(renderpass, nullptr, *device->getDispatcher());
        }
    }

    u32 RenderpassNode::AddAttachment(TextureView &view) {
        auto &textures{storage->textures};
        auto texture{std::find(textures.begin(), textures.end(), view.backing)};
        if (texture == textures.end())
            textures.push_back(view.backing);

        auto vkView{view.GetView()};
        auto attachment{std::find(attachments.begin(), attachments.end(), vkView)};
        if (attachment == attachments.end()) {
            // If we cannot find any matches for the specified attachment, we add it as a new one
            attachments.push_back(vkView);
            attachmentDescriptions.push_back(vk::AttachmentDescription{
                .format = *view.format,
                .initialLayout = view.backing->layout,
                .finalLayout = view.backing->layout,
            });
            return attachments.size() - 1;
        } else {
            // If we've got a match from a previous subpass, we need to preserve the attachment till the current subpass
            auto attachmentIndex{std::distance(attachments.begin(), attachment)};

            auto it{subpassDescriptions.begin()};
            for (; it != subpassDescriptions.end(); it++) {
                // Find the bounds for the attachment references belonging to the current subpass
                auto referenceBeginIt{attachmentReferences.begin()};
                referenceBeginIt += reinterpret_cast<uintptr_t>(it->pInputAttachments) / sizeof(vk::AttachmentReference);

                auto referenceEndIt{referenceBeginIt + it->inputAttachmentCount + it->colorAttachmentCount}; // We depend on all attachments being contiguous for a subpass, this will horribly break if that assumption is broken
                if (reinterpret_cast<uintptr_t>(it->pDepthStencilAttachment) != NoDepthStencil)
                    referenceEndIt++;

                // Iterate over all attachment references in the current subpass to see if they point to our target attachment
                if (std::find_if(referenceBeginIt, referenceEndIt, [&](const vk::AttachmentReference &reference) {
                    return reference.attachment == attachmentIndex;
                }) != referenceEndIt)
                    break; // The iterator should be set to the first subpass that utilizes the attachment we want to preserve
            }

            if (it == subpassDescriptions.end())
                throw exception("Cannot find corresponding subpass for attachment #{}", attachmentIndex);

            auto lastUsageIt{it}; //!< The last subpass that the attachment has been used in for creating a dependency
            for (; it != subpassDescriptions.end(); it++) {
                auto referenceBeginIt{attachmentReferences.begin()};
                referenceBeginIt += reinterpret_cast<uintptr_t>(it->pInputAttachments) / sizeof(vk::AttachmentReference);

                auto referenceEndIt{referenceBeginIt + it->inputAttachmentCount + it->colorAttachmentCount};
                if (reinterpret_cast<uintptr_t>(it->pDepthStencilAttachment) != NoDepthStencil)
                    referenceEndIt++;

                if (std::find_if(referenceBeginIt, referenceEndIt, [&](const vk::AttachmentReference &reference) {
                    return reference.attachment == attachmentIndex;
                }) != referenceEndIt) {
                    lastUsageIt = it;
                    continue; // If a subpass uses an attachment then it doesn't need to be preserved
                }

                auto &subpassPreserveAttachments{preserveAttachmentReferences[std::distance(subpassDescriptions.begin(), it)]};
                if (std::find(subpassPreserveAttachments.begin(), subpassPreserveAttachments.end(), attachmentIndex) != subpassPreserveAttachments.end())
                    subpassPreserveAttachments.push_back(attachmentIndex);
            }

            vk::SubpassDependency dependency{
                .srcSubpass = static_cast<u32>(std::distance(subpassDescriptions.begin(), lastUsageIt)),
                .dstSubpass = static_cast<uint32_t>(subpassDescriptions.size()), // We assume that the next subpass is using the attachment
                .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                .dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead,
                .dependencyFlags = vk::DependencyFlagBits::eByRegion,
            };

            if (std::find(subpassDependencies.begin(), subpassDependencies.end(), dependency) == subpassDependencies.end())
                subpassDependencies.push_back(dependency);

            return attachmentIndex;
        }
    }

    void RenderpassNode::AddSubpass(span<TextureView> inputAttachments, span<TextureView> colorAttachments, TextureView *depthStencilAttachment) {
        attachmentReferences.reserve(attachmentReferences.size() + inputAttachments.size() + colorAttachments.size() + (depthStencilAttachment ? 1 : 0));

        auto inputAttachmentsOffset{attachmentReferences.size() * sizeof(vk::AttachmentReference)};
        for (auto &attachment : inputAttachments) {
            attachmentReferences.push_back(vk::AttachmentReference{
                .attachment = AddAttachment(attachment),
                .layout = attachment.backing->layout,
            });
        }

        auto colorAttachmentsOffset{attachmentReferences.size() * sizeof(vk::AttachmentReference)}; // Calculate new base offset as it has changed since we pushed the input attachments
        for (auto &attachment : colorAttachments) {
            attachmentReferences.push_back(vk::AttachmentReference{
                .attachment = AddAttachment(attachment),
                .layout = attachment.backing->layout,
            });
        }

        auto depthStencilAttachmentOffset{attachmentReferences.size() * sizeof(vk::AttachmentReference)};
        if (depthStencilAttachment) {
            attachmentReferences.push_back(vk::AttachmentReference{
                .attachment = AddAttachment(*depthStencilAttachment),
                .layout = depthStencilAttachment->backing->layout,
            });
        }

        preserveAttachmentReferences.emplace_back(); // We need to create storage for any attachments that might need to preserved by this pass

        // Note: We encode the offsets as the pointers due to vector pointer invalidation, RebasePointer(...) can be utilized to deduce the real pointer
        subpassDescriptions.push_back(vk::SubpassDescription{
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .inputAttachmentCount = static_cast<u32>(inputAttachments.size()),
            .pInputAttachments = reinterpret_cast<vk::AttachmentReference *>(inputAttachmentsOffset),
            .colorAttachmentCount = static_cast<u32>(colorAttachments.size()),
            .pColorAttachments = reinterpret_cast<vk::AttachmentReference *>(colorAttachmentsOffset),
            .pDepthStencilAttachment = reinterpret_cast<vk::AttachmentReference *>(depthStencilAttachment ? depthStencilAttachmentOffset : NoDepthStencil),
        });
    }

    bool RenderpassNode::ClearColorAttachment(u32 colorAttachment, const vk::ClearColorValue &value) {
        auto attachmentReference{RebasePointer(attachmentReferences, subpassDescriptions.back().pColorAttachments) + colorAttachment};
        auto attachmentIndex{attachmentReference->attachment};

        for (const auto &reference : attachmentReferences)
            if (reference.attachment == attachmentIndex && &reference != attachmentReference)
                return false;

        auto &attachmentDescription{attachmentDescriptions.at(attachmentIndex)};
        if (attachmentDescription.loadOp == vk::AttachmentLoadOp::eLoad) {
            attachmentDescription.loadOp = vk::AttachmentLoadOp::eClear;

            clearValues.resize(attachmentIndex + 1);
            clearValues[attachmentIndex].color = value;

            return true;
        } else if (attachmentDescription.loadOp == vk::AttachmentLoadOp::eClear && clearValues[attachmentIndex].color.uint32 == value.uint32) {
            return true;
        }

        return false;
    }

    void RenderpassNode::operator()(vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle, GPU &gpu) {
        storage->device = &gpu.vkDevice;

        auto preserveAttachmentIt{preserveAttachmentReferences.begin()};
        for (auto &subpassDescription : subpassDescriptions) {
            subpassDescription.pInputAttachments = RebasePointer(attachmentReferences, subpassDescription.pInputAttachments);
            subpassDescription.pColorAttachments = RebasePointer(attachmentReferences, subpassDescription.pColorAttachments);

            auto depthStencilAttachmentOffset{reinterpret_cast<uintptr_t>(subpassDescription.pDepthStencilAttachment)};
            if (depthStencilAttachmentOffset != NoDepthStencil)
                subpassDescription.pDepthStencilAttachment = RebasePointer(attachmentReferences, subpassDescription.pDepthStencilAttachment);
            else
                subpassDescription.pDepthStencilAttachment = nullptr;

            subpassDescription.preserveAttachmentCount = preserveAttachmentIt->size();
            subpassDescription.pPreserveAttachments = preserveAttachmentIt->data();
            preserveAttachmentIt++;
        }

        for (auto &texture : storage->textures) {
            texture->lock();
            texture->WaitOnBacking();
            if (texture->cycle.lock() != cycle)
                texture->WaitOnFence();
        }

        auto renderpass{(*gpu.vkDevice).createRenderPass(vk::RenderPassCreateInfo{
            .attachmentCount = static_cast<u32>(attachmentDescriptions.size()),
            .pAttachments = attachmentDescriptions.data(),
            .subpassCount = static_cast<u32>(subpassDescriptions.size()),
            .pSubpasses = subpassDescriptions.data(),
            .dependencyCount = static_cast<u32>(subpassDependencies.size()),
            .pDependencies = subpassDependencies.data(),
        }, nullptr, *gpu.vkDevice.getDispatcher())};
        storage->renderpass = renderpass;

        auto framebuffer{(*gpu.vkDevice).createFramebuffer(vk::FramebufferCreateInfo{
            .renderPass = renderpass,
            .attachmentCount = static_cast<u32>(attachments.size()),
            .pAttachments = attachments.data(),
            .width = renderArea.extent.width,
            .height = renderArea.extent.height,
            .layers = 1,
        }, nullptr, *gpu.vkDevice.getDispatcher())};
        storage->framebuffer = framebuffer;

        commandBuffer.beginRenderPass(vk::RenderPassBeginInfo{
            .renderPass = renderpass,
            .framebuffer = framebuffer,
            .renderArea = renderArea,
            .clearValueCount = static_cast<u32>(clearValues.size()),
            .pClearValues = clearValues.data(),
        }, vk::SubpassContents::eInline);

        cycle->AttachObjects(storage);

        for (auto &texture : storage->textures) {
            texture->unlock();
            texture->cycle = cycle;
        }
    }
}
