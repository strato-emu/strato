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
        std::vector<std::vector<u32>> preserveAttachmentReferences; //!< Any attachment that must be preserved to be utilized by a future subpass, these are stored per-subpass to ensure contiguity

        constexpr static uintptr_t DepthStencilNull{std::numeric_limits<uintptr_t>::max()}; //!< A sentinel value to denote the lack of a depth stencil attachment in a VkSubpassDescription

        /**
         * @brief Rebases a pointer containing an offset relative to the beginning of a container
         */
        template<typename Container, typename T>
        T *RebasePointer(const Container &container, const T *offset) {
            return reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(container.data()) + reinterpret_cast<uintptr_t>(offset));
        }

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
                    auto referenceBeginIt{attachmentReferences.begin()};
                    referenceBeginIt += reinterpret_cast<uintptr_t>(it->pInputAttachments) / sizeof(vk::AttachmentReference);

                    auto referenceEndIt{referenceBeginIt + it->inputAttachmentCount + it->colorAttachmentCount}; // We depend on all attachments being contiguous for a subpass, this will horribly break if that assumption is broken
                    if (reinterpret_cast<uintptr_t>(it->pDepthStencilAttachment) != DepthStencilNull)
                        referenceEndIt++;

                    if (std::find_if(referenceBeginIt, referenceEndIt, [&](const vk::AttachmentReference &reference) {
                        return reference.attachment == attachmentIndex;
                    }) != referenceEndIt)
                        break; // The first subpass that utilizes the attachment we want to preserve
                }

                if (it == subpassDescriptions.end())
                    throw exception("Cannot find corresponding subpass for attachment #{}", attachmentIndex);

                auto lastUsageIt{it};
                for (; it != subpassDescriptions.end(); it++) {
                    auto referenceBeginIt{attachmentReferences.begin()};
                    referenceBeginIt += reinterpret_cast<uintptr_t>(it->pInputAttachments) / sizeof(vk::AttachmentReference);

                    auto referenceEndIt{referenceBeginIt + it->inputAttachmentCount + it->colorAttachmentCount};
                    if (reinterpret_cast<uintptr_t>(it->pDepthStencilAttachment) != DepthStencilNull)
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

        /**
         * @brief Creates a subpass with the attachments bound in the specified order
         */
        void AddSubpass(span <TextureView> inputAttachments, span <TextureView> colorAttachments, TextureView *depthStencilAttachment) {
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

            // Note: We encode the offsets as the pointers due to vector pointer invalidation, RebasePointer(...) can be utilized to deduce the real pointer
            subpassDescriptions.push_back(vk::SubpassDescription{
                .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                .inputAttachmentCount = static_cast<u32>(inputAttachments.size()),
                .pInputAttachments = reinterpret_cast<vk::AttachmentReference *>(inputAttachmentsOffset),
                .colorAttachmentCount = static_cast<u32>(colorAttachments.size()),
                .pColorAttachments = reinterpret_cast<vk::AttachmentReference *>(colorAttachmentsOffset),
                .pDepthStencilAttachment = reinterpret_cast<vk::AttachmentReference *>(depthStencilAttachment ? depthStencilAttachmentOffset : DepthStencilNull),
            });
        }

        /**
         * @brief Clears a color attachment in the current subpass with VK_ATTACHMENT_LOAD_OP_LOAD
         * @param colorAttachment The index of the attachment in the attachments bound to the current subpass
         * @return If the attachment could be cleared or not due to conflicts with other operations
         * @note We require a subpass to be attached during this as the clear will not take place unless it's referenced by a subpass
         */
        bool ClearColorAttachment(u32 colorAttachment, const vk::ClearColorValue &value) {
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

        void operator()(vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle, GPU &gpu) {
            storage->device = &gpu.vkDevice;

            auto preserveAttachmentIt{preserveAttachmentReferences.begin()};
            for (auto &subpassDescription : subpassDescriptions) {
                subpassDescription.pInputAttachments = RebasePointer(attachmentReferences, subpassDescription.pInputAttachments);
                subpassDescription.pColorAttachments = RebasePointer(attachmentReferences, subpassDescription.pColorAttachments);

                auto depthStencilAttachmentOffset{reinterpret_cast<uintptr_t>(subpassDescription.pDepthStencilAttachment)};
                if (depthStencilAttachmentOffset != DepthStencilNull)
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
    };

    /**
     * @brief A FunctionNode which progresses to the next subpass prior to calling the function
     */
    struct NextSubpassNode : private FunctionNode {
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
