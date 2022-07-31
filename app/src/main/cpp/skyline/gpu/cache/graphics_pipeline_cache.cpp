// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <boost/functional/hash.hpp>
#include <gpu.h>
#include "graphics_pipeline_cache.h"

namespace skyline::gpu::cache {
    GraphicsPipelineCache::GraphicsPipelineCache(GPU &gpu) : gpu(gpu), vkPipelineCache(gpu.vkDevice, vk::PipelineCacheCreateInfo{}) {}

    #define VEC_CPY(pointer, size) state.pointer, state.pointer + state.size

    GraphicsPipelineCache::PipelineCacheKey::PipelineCacheKey(const GraphicsPipelineCache::PipelineState &state)
        : shaderStages(state.shaderStages.begin(), state.shaderStages.end()),
          vertexState(state.vertexState),
          vertexBindings(VEC_CPY(VertexInputState().pVertexBindingDescriptions, VertexInputState().vertexBindingDescriptionCount)),
          vertexAttributes(VEC_CPY(VertexInputState().pVertexAttributeDescriptions, VertexInputState().vertexAttributeDescriptionCount)),
          vertexDivisors(VEC_CPY(VertexDivisorState().pVertexBindingDivisors, VertexDivisorState().vertexBindingDivisorCount)),
          inputAssemblyState(state.inputAssemblyState),
          tessellationState(state.tessellationState),
          viewportState(state.viewportState),
          viewports(VEC_CPY(viewportState.pViewports, viewportState.viewportCount)),
          scissors(VEC_CPY(viewportState.pScissors, viewportState.scissorCount)),
          rasterizationState(state.rasterizationState),
          multisampleState(state.multisampleState),
          depthStencilState(state.depthStencilState),
          colorBlendState(state.colorBlendState),
          colorBlendAttachments(VEC_CPY(colorBlendState.pAttachments, colorBlendState.attachmentCount)) {
        auto &vertexInputState{vertexState.get<vk::PipelineVertexInputStateCreateInfo>()};
        vertexInputState.pVertexBindingDescriptions = vertexBindings.data();
        vertexInputState.pVertexAttributeDescriptions = vertexAttributes.data();
        vertexState.get<vk::PipelineVertexInputDivisorStateCreateInfoEXT>().pVertexBindingDivisors = vertexDivisors.data();

        viewportState.pViewports = viewports.data();
        viewportState.pScissors = scissors.data();

        colorBlendState.pAttachments = colorBlendAttachments.data();

        for (auto &colorAttachment : state.colorAttachments)
            colorAttachments.emplace_back(AttachmentMetadata{colorAttachment->format->vkFormat, colorAttachment->texture->sampleCount});
        if (state.depthStencilAttachment)
            depthStencilAttachment.emplace(AttachmentMetadata{state.depthStencilAttachment->format->vkFormat, state.depthStencilAttachment->texture->sampleCount});
    }

    #undef VEC_CPY

    #define HASH(x) boost::hash_combine(hash, x)

    template<typename T>
    size_t HashCommonPipelineState(const T &key, size_t hash = 0) {
        HASH(key.shaderStages.size());
        for (const auto &stage : key.shaderStages) {
            HASH(stage.stage);
            HASH(static_cast<VkShaderModule>(stage.module));
        }

        auto &vertexInputState{key.VertexInputState()};
        HASH(vertexInputState.vertexBindingDescriptionCount);
        HASH(vertexInputState.vertexAttributeDescriptionCount);
        HASH(static_cast<VkFlags>(vertexInputState.flags));

        for (size_t i{}; i < vertexInputState.vertexBindingDescriptionCount; i++) {
            const auto &descr{vertexInputState.pVertexBindingDescriptions[i]};
            HASH(descr.binding);
            HASH(descr.stride);
            HASH(static_cast<VkVertexInputRate>(descr.inputRate));
        }

        for (size_t i{}; i < vertexInputState.vertexAttributeDescriptionCount; i++) {
            const auto &descr{vertexInputState.pVertexAttributeDescriptions[i]};
            HASH(descr.binding);
            HASH(descr.offset);
            HASH(descr.location);
            HASH(static_cast<VkFormat>(descr.format));
        }

        if (key.vertexState.template isLinked<vk::PipelineVertexInputDivisorStateCreateInfoEXT>())
            HASH(key.VertexDivisorState().vertexBindingDivisorCount);

        HASH(key.inputAssemblyState.topology);
        HASH(key.inputAssemblyState.primitiveRestartEnable);

        HASH(key.tessellationState.patchControlPoints);

        HASH(key.viewportState.viewportCount);
        HASH(key.viewportState.scissorCount);

        for (size_t i{}; i < key.viewportState.viewportCount; i++) {
            const auto &viewport{key.viewportState.pViewports[i]};
            HASH(viewport.x);
            HASH(viewport.y);
            HASH(viewport.width);
            HASH(viewport.height);
            HASH(viewport.minDepth);
            HASH(viewport.maxDepth);
        }

        for (size_t i{}; i < key.viewportState.scissorCount; i++) {
            const auto &scissor{key.viewportState.pScissors[i]};
            HASH(scissor.offset.x);
            HASH(scissor.offset.y);
            HASH(scissor.extent.width);
            HASH(scissor.extent.height);
        }

        auto &rasterizationState{key.RasterizationState()};
        HASH(rasterizationState.depthClampEnable);
        HASH(rasterizationState.rasterizerDiscardEnable);
        HASH(rasterizationState.polygonMode);
        HASH(std::hash<vk::CullModeFlags>{}(rasterizationState.cullMode));
        HASH(rasterizationState.frontFace);
        HASH(rasterizationState.depthBiasEnable);
        HASH(rasterizationState.depthBiasConstantFactor);
        HASH(rasterizationState.depthBiasClamp);
        HASH(rasterizationState.depthBiasSlopeFactor);
        HASH(rasterizationState.lineWidth);

        if (key.rasterizationState.template isLinked<vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT>())
            HASH(key.ProvokingVertexState().provokingVertexMode);

        HASH(key.multisampleState.rasterizationSamples);
        HASH(key.multisampleState.sampleShadingEnable);
        HASH(key.multisampleState.minSampleShading);
        HASH(key.multisampleState.alphaToCoverageEnable);
        HASH(key.multisampleState.alphaToOneEnable);

        HASH(key.depthStencilState.depthTestEnable);
        HASH(key.depthStencilState.depthWriteEnable);
        HASH(key.depthStencilState.depthCompareOp);
        HASH(key.depthStencilState.depthBoundsTestEnable);
        HASH(key.depthStencilState.stencilTestEnable);
        HASH(key.depthStencilState.front.compareOp);
        HASH(key.depthStencilState.front.failOp);
        HASH(key.depthStencilState.front.passOp);
        HASH(key.depthStencilState.front.depthFailOp);
        HASH(key.depthStencilState.front.compareMask);
        HASH(key.depthStencilState.front.writeMask);
        HASH(key.depthStencilState.front.reference);
        HASH(key.depthStencilState.back.compareOp);
        HASH(key.depthStencilState.back.failOp);
        HASH(key.depthStencilState.back.passOp);
        HASH(key.depthStencilState.back.depthFailOp);
        HASH(key.depthStencilState.back.compareMask);
        HASH(key.depthStencilState.back.writeMask);
        HASH(key.depthStencilState.back.reference);
        HASH(key.depthStencilState.minDepthBounds);
        HASH(key.depthStencilState.maxDepthBounds);

        HASH(key.colorBlendState.logicOpEnable);
        HASH(key.colorBlendState.logicOp);
        HASH(key.colorBlendState.attachmentCount);

        for (size_t i{}; i < key.colorBlendState.attachmentCount; i++) {
            const auto &attachment{key.colorBlendState.pAttachments[i]};
            HASH(static_cast<VkBool32>(attachment.blendEnable));

            HASH(static_cast<VkBlendOp>(attachment.alphaBlendOp));
            HASH(static_cast<VkBlendOp>(attachment.colorBlendOp));

            HASH(static_cast<VkBlendFactor>(attachment.dstAlphaBlendFactor));
            HASH(static_cast<VkBlendFactor>(attachment.dstColorBlendFactor));
            HASH(static_cast<VkBlendFactor>(attachment.srcAlphaBlendFactor));
            HASH(static_cast<VkBlendFactor>(attachment.srcColorBlendFactor));
        }

        return hash;
    }

    size_t GraphicsPipelineCache::PipelineStateHash::operator()(const GraphicsPipelineCache::PipelineState &key) const {
        size_t hash{HashCommonPipelineState(key)};

        HASH(key.colorAttachments.size());
        for (const auto &attachment : key.colorAttachments) {
            HASH(attachment->format->vkFormat);
            HASH(attachment->texture->sampleCount);
        }

        HASH(key.depthStencilAttachment != nullptr);
        if (key.depthStencilAttachment != nullptr) {
            HASH(key.depthStencilAttachment->format->vkFormat);
            HASH(key.depthStencilAttachment->texture->sampleCount);
        }

        return hash;
    }

    size_t GraphicsPipelineCache::PipelineStateHash::operator()(const GraphicsPipelineCache::PipelineCacheKey &key) const {
        size_t hash{HashCommonPipelineState(key)};

        HASH(key.colorAttachments.size());
        for (const auto &attachment : key.colorAttachments) {
            HASH(attachment.format);
            HASH(attachment.sampleCount);
        }

        HASH(key.depthStencilAttachment.has_value());
        if (key.depthStencilAttachment) {
            HASH(key.depthStencilAttachment->format);
            HASH(key.depthStencilAttachment->sampleCount);
        }

        return hash;
    }

    #undef HASH

    bool GraphicsPipelineCache::PipelineCacheEqual::operator()(const GraphicsPipelineCache::PipelineCacheKey &lhs, const GraphicsPipelineCache::PipelineState &rhs) const {
        #define RETF(condition) if (condition) { return false; }
        #define KEYEQ(member) (lhs.member == rhs.member)
        #define KEYNEQ(member) (lhs.member != rhs.member)
        static constexpr auto NotEqual{[](auto pointer, auto size, auto pointer2, auto size2, auto equalFunction) -> bool {
            return
                size != size2 ||
                    !std::equal(pointer, pointer + static_cast<ssize_t>(size), pointer2, equalFunction);
        }};
        #define CARREQ(pointer, size, func) NotEqual(lhs.pointer, lhs.size, rhs.pointer, rhs.size, [](decltype(*lhs.pointer) &lhs, decltype(*rhs.pointer) &rhs) { func }) // Note: typeof(*lhs/rhs.pointer) is required for clangd to resolve the parameter type correctly for autocomplete
        #define ARREQ(pointer, size) CARREQ(pointer, size, { return lhs == rhs; })

        RETF(CARREQ(shaderStages.begin(), shaderStages.size(), {
            return KEYEQ(flags) && KEYEQ(stage) && KEYEQ(module) && std::strcmp(lhs.pName, rhs.pName) == 0;
            // Note: We intentionally ignore specialization constants here
        }))

        RETF(KEYNEQ(VertexInputState().flags) ||
            ARREQ(VertexInputState().pVertexBindingDescriptions, VertexInputState().vertexBindingDescriptionCount) ||
            ARREQ(VertexInputState().pVertexAttributeDescriptions, VertexInputState().vertexAttributeDescriptionCount)
        )

        RETF(KEYNEQ(vertexState.isLinked<vk::PipelineVertexInputDivisorStateCreateInfoEXT>()) ||
            (lhs.vertexState.isLinked<vk::PipelineVertexInputDivisorStateCreateInfoEXT>() &&
                ARREQ(VertexDivisorState().pVertexBindingDivisors, VertexDivisorState().vertexBindingDivisorCount)
            )
        )

        RETF(KEYNEQ(tessellationState.flags) || KEYNEQ(tessellationState.patchControlPoints))

        RETF(KEYNEQ(inputAssemblyState.flags) || KEYNEQ(inputAssemblyState.topology) || KEYNEQ(inputAssemblyState.primitiveRestartEnable))

        RETF(KEYNEQ(viewportState.flags) ||
            ARREQ(viewportState.pViewports, viewportState.viewportCount) ||
            ARREQ(viewportState.pScissors, viewportState.scissorCount)
        )

        RETF(KEYNEQ(RasterizationState().flags) ||
            KEYNEQ(RasterizationState().depthClampEnable) ||
            KEYNEQ(RasterizationState().rasterizerDiscardEnable) ||
            KEYNEQ(RasterizationState().polygonMode) ||
            KEYNEQ(RasterizationState().cullMode) ||
            KEYNEQ(RasterizationState().frontFace) ||
            KEYNEQ(RasterizationState().depthBiasEnable) ||
            KEYNEQ(RasterizationState().depthBiasConstantFactor) ||
            KEYNEQ(RasterizationState().depthBiasClamp) ||
            KEYNEQ(RasterizationState().depthBiasSlopeFactor) ||
            KEYNEQ(RasterizationState().lineWidth)
        )

        RETF(KEYNEQ(rasterizationState.isLinked<vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT>()) ||
            (lhs.rasterizationState.isLinked<vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT>() &&
                KEYNEQ(ProvokingVertexState().provokingVertexMode)
            )
        )

        RETF(KEYNEQ(multisampleState.flags) ||
            KEYNEQ(multisampleState.rasterizationSamples) ||
            KEYNEQ(multisampleState.sampleShadingEnable) ||
            KEYNEQ(multisampleState.minSampleShading) ||
            KEYNEQ(multisampleState.alphaToCoverageEnable) ||
            KEYNEQ(multisampleState.alphaToOneEnable)
        )

        RETF(KEYNEQ(depthStencilState.flags) ||
            KEYNEQ(depthStencilState.depthTestEnable) ||
            KEYNEQ(depthStencilState.depthWriteEnable) ||
            KEYNEQ(depthStencilState.depthCompareOp) ||
            KEYNEQ(depthStencilState.depthBoundsTestEnable) ||
            KEYNEQ(depthStencilState.stencilTestEnable) ||
            KEYNEQ(depthStencilState.front) ||
            KEYNEQ(depthStencilState.back) ||
            KEYNEQ(depthStencilState.minDepthBounds) ||
            KEYNEQ(depthStencilState.maxDepthBounds)
        )

        RETF(KEYNEQ(colorBlendState.flags) ||
            KEYNEQ(colorBlendState.logicOpEnable) ||
            KEYNEQ(colorBlendState.logicOp) ||
            ARREQ(colorBlendState.pAttachments, colorBlendState.attachmentCount) ||
            KEYNEQ(colorBlendState.blendConstants)
        )

        RETF(CARREQ(colorAttachments.begin(), colorAttachments.size(), {
            return lhs.format == rhs->format->vkFormat && lhs.sampleCount == rhs->texture->sampleCount;
        }))

        RETF(lhs.depthStencilAttachment.has_value() != (rhs.depthStencilAttachment != nullptr) ||
            (lhs.depthStencilAttachment.has_value() &&
                lhs.depthStencilAttachment->format != rhs.depthStencilAttachment->format->vkFormat &&
                lhs.depthStencilAttachment->sampleCount != rhs.depthStencilAttachment->texture->sampleCount
            )
        )

        #undef ARREQ
        #undef CARREQ
        #undef KEYNEQ
        #undef KEYEQ
        #undef RETF

        return true;
    }

    bool GraphicsPipelineCache::PipelineCacheEqual::operator()(const PipelineCacheKey &lhs, const PipelineCacheKey &rhs) const {
        return lhs == rhs;
    }

    GraphicsPipelineCache::PipelineCacheEntry::PipelineCacheEntry(vk::raii::DescriptorSetLayout &&descriptorSetLayout, vk::raii::PipelineLayout &&pipelineLayout, vk::raii::Pipeline &&pipeline) : descriptorSetLayout(std::move(descriptorSetLayout)), pipelineLayout(std::move(pipelineLayout)), pipeline(std::move(pipeline)) {}

    GraphicsPipelineCache::CompiledPipeline::CompiledPipeline(const PipelineCacheEntry &entry) : descriptorSetLayout(*entry.descriptorSetLayout), pipelineLayout(*entry.pipelineLayout), pipeline(*entry.pipeline) {}

    GraphicsPipelineCache::CompiledPipeline GraphicsPipelineCache::GetCompiledPipeline(const PipelineState &state, span<const vk::DescriptorSetLayoutBinding> layoutBindings, span<const vk::PushConstantRange> pushConstantRanges) {
        std::unique_lock lock(mutex);

        auto it{pipelineCache.find(state)};
        if (it != pipelineCache.end())
            return CompiledPipeline{it->second};

        lock.unlock();

        vk::raii::DescriptorSetLayout descriptorSetLayout{gpu.vkDevice, vk::DescriptorSetLayoutCreateInfo{
            .flags = gpu.traits.supportsPushDescriptors ? vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR : vk::DescriptorSetLayoutCreateFlags{},
            .pBindings = layoutBindings.data(),
            .bindingCount = static_cast<u32>(layoutBindings.size()),
        }};

        vk::raii::PipelineLayout pipelineLayout{gpu.vkDevice, vk::PipelineLayoutCreateInfo{
            .pSetLayouts = &*descriptorSetLayout,
            .setLayoutCount = 1,
            .pPushConstantRanges = pushConstantRanges.data(),
            .pushConstantRangeCount = static_cast<u32>(pushConstantRanges.size()),
        }};

        boost::container::small_vector<vk::AttachmentDescription, 8> attachmentDescriptions;
        boost::container::small_vector<vk::AttachmentReference, 8> attachmentReferences;

        auto pushAttachment{[&](const TextureView &view) {
            attachmentDescriptions.push_back(vk::AttachmentDescription{
                .format = view.format->vkFormat,
                .samples = view.texture->sampleCount,
                .loadOp = vk::AttachmentLoadOp::eLoad,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eLoad,
                .stencilStoreOp = vk::AttachmentStoreOp::eStore,
                .initialLayout = view.texture->layout,
                .finalLayout = view.texture->layout,
            });
            attachmentReferences.push_back(vk::AttachmentReference{
                .attachment = static_cast<u32>(attachmentDescriptions.size() - 1),
                .layout = view.texture->layout,
            });
        }};

        vk::SubpassDescription subpassDescription{
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        };

        for (auto &colorAttachment : state.colorAttachments)
            pushAttachment(*colorAttachment);

        if (state.depthStencilAttachment) {
            pushAttachment(*state.depthStencilAttachment);

            subpassDescription.pColorAttachments = attachmentReferences.data();
            subpassDescription.colorAttachmentCount = static_cast<u32>(attachmentReferences.size() - 1);
            subpassDescription.pDepthStencilAttachment = &attachmentReferences.back();
        } else {
            subpassDescription.pColorAttachments = attachmentReferences.data();
            subpassDescription.colorAttachmentCount = static_cast<u32>(attachmentReferences.size());
        }

        vk::raii::RenderPass renderPass{gpu.vkDevice, vk::RenderPassCreateInfo{
            .attachmentCount = static_cast<u32>(attachmentDescriptions.size()),
            .pAttachments = attachmentDescriptions.data(),
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
        }};

        auto pipeline{gpu.vkDevice.createGraphicsPipeline(vkPipelineCache, vk::GraphicsPipelineCreateInfo{
            .pStages = state.shaderStages.data(),
            .stageCount = static_cast<u32>(state.shaderStages.size()),
            .pVertexInputState = &state.vertexState.get<vk::PipelineVertexInputStateCreateInfo>(),
            .pInputAssemblyState = &state.inputAssemblyState,
            .pViewportState = &state.viewportState,
            .pRasterizationState = &state.rasterizationState.get<vk::PipelineRasterizationStateCreateInfo>(),
            .pMultisampleState = &state.multisampleState,
            .pDepthStencilState = &state.depthStencilState,
            .pColorBlendState = &state.colorBlendState,
            .pDynamicState = nullptr,
            .layout = *pipelineLayout,
            .renderPass = *renderPass,
            .subpass = 0,
        })};

        lock.lock();

        auto pipelineEntryIt{pipelineCache.try_emplace(PipelineCacheKey{state}, std::move(descriptorSetLayout), std::move(pipelineLayout), std::move(pipeline))};
        return CompiledPipeline{pipelineEntryIt.first->second};
    }
}
