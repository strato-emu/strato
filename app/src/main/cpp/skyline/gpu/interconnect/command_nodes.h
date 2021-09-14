// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <gpu.h>

namespace skyline::gpu::interconnect::node {
    /**
     * @brief A generic node for simply executing a function
     */
    template<typename FunctionSignature = void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &)>
    struct FunctionNodeBase {
        std::function<FunctionSignature> function;

        FunctionNodeBase(std::function<FunctionSignature> function) : function(function) {}

        template<class... Args>
        void operator()(Args &&... args) {
            function(std::forward<Args>(args)...);
        }
    };

    using FunctionNode = FunctionNodeBase<>;

    /**
     * @brief Creates and begins a VkRenderpass while tying lifetimes of all bound resources to a GPU fence
     */
    struct RenderpassNode {
      private:
        struct Storage : public FenceCycleDependency {
            vk::raii::Device *device{};
            vk::Framebuffer framebuffer{};
            vk::RenderPass renderpass{};
            std::vector<std::shared_ptr<Texture>> textures;

            ~Storage() {
                if (device) {
                    if (framebuffer)
                        (**device).destroy(framebuffer, nullptr, *device->getDispatcher());
                    if (renderpass)
                        (**device).destroy(renderpass, nullptr, *device->getDispatcher());
                }
            }
        };
        std::shared_ptr<Storage> storage;

        std::vector<vk::ImageView> attachments;
        std::vector<vk::AttachmentDescription> attachmentDescriptions;

        std::vector<vk::AttachmentReference> attachmentReferences;
        std::vector<boost::container::small_vector<u32, 5>> preserveAttachmentReferences; //!< Any attachment that must be preserved to be utilized by a future subpass, these are stored per-subpass to ensure contiguity

      public:
        std::vector<vk::SubpassDescription> subpassDescriptions;
        std::vector<vk::SubpassDependency> subpassDependencies;

        vk::Rect2D renderArea;
        std::vector<vk::ClearValue> clearValues;

        RenderpassNode(vk::Rect2D renderArea) : storage(std::make_shared<Storage>()), renderArea(renderArea) {}

        /**
         * @note Any preservation of attachments from previous subpasses is automatically handled by this
         * @return The index of the attachment in the render pass which can be utilized with VkAttachmentReference
         */
        u32 AddAttachment(TextureView &view) {
            auto &textures{storage->textures};
            auto texture{std::find(textures.begin(), textures.end(), view.backing)};
            if (texture == textures.end())
                textures.push_back(view.backing);

            vk::AttachmentDescription attachmentDescription{
                .format = *view.format,
                .initialLayout = view.backing->layout,
                .finalLayout = view.backing->layout,
            };

            auto vkView{view.GetView()};
            auto attachment{std::find(attachments.begin(), attachments.end(), vkView)};
            if (attachment == attachments.end() || attachmentDescriptions[std::distance(attachments.begin(), attachment)] != attachmentDescription) {
                // If we cannot find any matches for the specified attachment, we add it as a new one
                attachments.push_back(vkView);
                attachmentDescriptions.push_back(attachmentDescription);
                return attachments.size() - 1;
            } else {
                // If we've got a match from a previous subpass, we need to preserve the attachment till the current subpass
                auto attachmentIndex{std::distance(attachments.begin(), attachment)};
                auto attachmentReferenceIt{std::find_if(attachmentReferences.begin(), attachmentReferences.end(), [&](const vk::AttachmentReference &reference) {
                    return reference.attachment == attachmentIndex;
                })};

                auto attachmentReferenceOffset{std::distance(attachmentReferences.begin(), attachmentReferenceIt) * sizeof(vk::AttachmentReference)};
                auto subpassDescriptionIt{std::find_if(subpassDescriptions.begin(), subpassDescriptions.end(), [&](const vk::SubpassDescription &description) {
                    return reinterpret_cast<uintptr_t>(description.pDepthStencilAttachment) > attachmentReferenceOffset;
                })};

                for (ssize_t subpassIndex{std::distance(subpassDescriptions.begin(), subpassDescriptionIt)}; subpassIndex != subpassDescriptions.size(); subpassIndex++)
                    preserveAttachmentReferences[subpassIndex].push_back(attachmentIndex);

                return std::distance(attachments.begin(), attachment);
            }
        }

        void AddSubpass(std::vector<TextureView> &inputAttachments, std::vector<TextureView> &colorAttachments, std::optional<TextureView> &depthStencilAttachment) {
            attachmentReferences.reserve(attachmentReferences.size() + inputAttachments.size() + colorAttachments.size() + (depthStencilAttachment ? 1 : 0));

            auto inputAttachmentsOffset{attachmentReferences.size() * sizeof(vk::AttachmentReference)};
            for (auto &attachment : inputAttachments) {
                attachmentReferences.push_back(vk::AttachmentReference{
                    .attachment = AddAttachment(attachment),
                    .layout = attachment.backing->layout,
                });
            }

            auto colorAttachmentsOffset{attachmentReferences.size() * sizeof(vk::AttachmentReference)};
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

            // Note: We encode the offsets as the pointers due to vector pointer invalidation, the vector offset will be added to them prior to submission
            subpassDescriptions.push_back(vk::SubpassDescription{
                .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                .inputAttachmentCount = static_cast<u32>(inputAttachments.size()),
                .pInputAttachments = reinterpret_cast<vk::AttachmentReference *>(inputAttachmentsOffset),
                .colorAttachmentCount = static_cast<u32>(colorAttachments.size()),
                .pColorAttachments = reinterpret_cast<vk::AttachmentReference *>(colorAttachmentsOffset),
                .pDepthStencilAttachment = reinterpret_cast<vk::AttachmentReference *>(depthStencilAttachment ? depthStencilAttachmentOffset : std::numeric_limits<uintptr_t>::max()),
            });
        }

        void operator()(vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle, GPU &gpu) {
            storage->device = &gpu.vkDevice;

            auto preserveAttachmentIt{preserveAttachmentReferences.begin()};
            auto attachmentReferenceOffset{reinterpret_cast<uintptr_t>(attachmentReferences.data())};
            for (auto &subpassDescription : subpassDescriptions) {
                subpassDescription.pInputAttachments = reinterpret_cast<vk::AttachmentReference *>(attachmentReferenceOffset + reinterpret_cast<uintptr_t>(subpassDescription.pInputAttachments));
                subpassDescription.pColorAttachments = reinterpret_cast<vk::AttachmentReference *>(attachmentReferenceOffset + reinterpret_cast<uintptr_t>(subpassDescription.pColorAttachments));

                auto depthStencilAttachmentOffset{reinterpret_cast<uintptr_t>(subpassDescription.pDepthStencilAttachment)};
                if (depthStencilAttachmentOffset != std::numeric_limits<uintptr_t>::max())
                    subpassDescription.pDepthStencilAttachment = reinterpret_cast<vk::AttachmentReference *>(attachmentReferenceOffset + depthStencilAttachmentOffset);
                else
                    subpassDescription.pDepthStencilAttachment = nullptr;

                subpassDescription.preserveAttachmentCount = preserveAttachmentIt->size();
                subpassDescription.pPreserveAttachments = preserveAttachmentIt->data();
                preserveAttachmentIt++;
            }

            for (auto &texture : storage->textures) {
                texture->lock();
                texture->WaitOnBacking();
                if (texture->cycle != cycle)
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
    };

    /**
     * @brief A FunctionNode which progresses to the next subpass prior to calling the function
     */
    struct NextSubpassNode : FunctionNode {
        using FunctionNode::FunctionNode;

        void operator()(vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle, GPU &gpu) {
            commandBuffer.nextSubpass(vk::SubpassContents::eInline);
            FunctionNode::operator()(commandBuffer, cycle, gpu);
        }
    };

    /**
     * @brief Ends a VkRenderpass that would be created prior with RenderpassNode
     */
    struct RenderpassEndNode {
        void operator()(vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle, GPU &gpu) {
            commandBuffer.endRenderPass();
        }
    };

    using NodeVariant = std::variant<FunctionNode, RenderpassNode, NextSubpassNode, RenderpassEndNode>; //!< A variant encompassing all command nodes types
}
