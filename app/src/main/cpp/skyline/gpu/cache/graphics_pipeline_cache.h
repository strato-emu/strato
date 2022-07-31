// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace skyline::gpu {
    class TextureView;
}

namespace skyline::gpu::cache {
    /**
     * @brief A cache for all Vulkan graphics pipelines objects used by the GPU to avoid costly re-creation
     * @note The cache is **not** compliant with Vulkan specification's Render Pass Compatibility clause when used with multi-subpass Render Passes but certain drivers may support a more relaxed version of this clause in practice which may allow it to be used with multi-subpass Render Passes
     */
    class GraphicsPipelineCache {
      public:
        /**
         * @brief All unique state required to compile a graphics pipeline as references
         */
        struct PipelineState {
            span<vk::PipelineShaderStageCreateInfo> shaderStages;
            const vk::StructureChain<vk::PipelineVertexInputStateCreateInfo, vk::PipelineVertexInputDivisorStateCreateInfoEXT> &vertexState;
            const vk::PipelineInputAssemblyStateCreateInfo &inputAssemblyState;
            const vk::PipelineTessellationStateCreateInfo &tessellationState;
            const vk::PipelineViewportStateCreateInfo &viewportState;
            const vk::StructureChain<vk::PipelineRasterizationStateCreateInfo, vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT> &rasterizationState;
            const vk::PipelineMultisampleStateCreateInfo &multisampleState;
            const vk::PipelineDepthStencilStateCreateInfo &depthStencilState;
            const vk::PipelineColorBlendStateCreateInfo &colorBlendState;

            span<TextureView *> colorAttachments; //!< All color attachments in the subpass of this pipeline
            TextureView *depthStencilAttachment; //!< A nullable pointer to the depth/stencil attachment in the subpass of this pipeline

            constexpr const vk::PipelineVertexInputStateCreateInfo &VertexInputState() const {
                return vertexState.get<vk::PipelineVertexInputStateCreateInfo>();
            }

            constexpr const vk::PipelineVertexInputDivisorStateCreateInfoEXT &VertexDivisorState() const {
                return vertexState.get<vk::PipelineVertexInputDivisorStateCreateInfoEXT>();
            }

            constexpr const vk::PipelineRasterizationStateCreateInfo &RasterizationState() const {
                return rasterizationState.get<vk::PipelineRasterizationStateCreateInfo>();
            }

            constexpr const vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT &ProvokingVertexState() const {
                return rasterizationState.get<vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT>();
            }
        };

      private:
        GPU &gpu;
        std::mutex mutex; //!< Synchronizes accesses to the pipeline cache
        vk::raii::PipelineCache vkPipelineCache; //!< A Vulkan Pipeline Cache which stores all unique graphics pipelines

        /**
         * @brief All unique metadata in a single attachment for a compatible render pass according to Render Pass Compatibility clause in the Vulkan specification
         * @url https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#renderpass-compatibility
         * @url https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkAttachmentDescription.html
         */
        struct AttachmentMetadata {
            vk::Format format;
            vk::SampleCountFlagBits sampleCount;

            constexpr AttachmentMetadata(vk::Format format, vk::SampleCountFlagBits sampleCount) : format(format), sampleCount(sampleCount) {}

            bool operator==(const AttachmentMetadata &rhs) const = default;
        };

        /**
         * @brief All data in PipelineState in value form to allow cheap heterogenous lookups with reference types while still storing a value-based key in the map
         */
        struct PipelineCacheKey {
            std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
            vk::StructureChain<vk::PipelineVertexInputStateCreateInfo, vk::PipelineVertexInputDivisorStateCreateInfoEXT> vertexState;
            std::vector<vk::VertexInputBindingDescription> vertexBindings;
            std::vector<vk::VertexInputAttributeDescription> vertexAttributes;
            std::vector<vk::VertexInputBindingDivisorDescriptionEXT> vertexDivisors;
            vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState;
            vk::PipelineTessellationStateCreateInfo tessellationState;
            vk::PipelineViewportStateCreateInfo viewportState;
            std::vector<vk::Viewport> viewports;
            std::vector<vk::Rect2D> scissors;
            vk::StructureChain<vk::PipelineRasterizationStateCreateInfo, vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT> rasterizationState;
            vk::PipelineMultisampleStateCreateInfo multisampleState;
            vk::PipelineDepthStencilStateCreateInfo depthStencilState;
            vk::PipelineColorBlendStateCreateInfo colorBlendState;
            std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments;

            std::vector<AttachmentMetadata> colorAttachments;
            std::optional<AttachmentMetadata> depthStencilAttachment;

            PipelineCacheKey(const PipelineState& state);

            bool operator==(const PipelineCacheKey& other) const = default;

            constexpr const vk::PipelineVertexInputStateCreateInfo &VertexInputState() const {
                return vertexState.get<vk::PipelineVertexInputStateCreateInfo>();
            }

            constexpr const vk::PipelineVertexInputDivisorStateCreateInfoEXT &VertexDivisorState() const {
                return vertexState.get<vk::PipelineVertexInputDivisorStateCreateInfoEXT>();
            }

            constexpr const vk::PipelineRasterizationStateCreateInfo &RasterizationState() const {
                return rasterizationState.get<vk::PipelineRasterizationStateCreateInfo>();
            }

            constexpr const vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT &ProvokingVertexState() const {
                return rasterizationState.get<vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT>();
            }
        };

        struct PipelineStateHash {
            using is_transparent = std::true_type;

            size_t operator()(const PipelineState &key) const;

            size_t operator()(const PipelineCacheKey &key) const;
        };

        struct PipelineCacheEqual {
            using is_transparent = std::true_type;

            bool operator()(const PipelineCacheKey &lhs, const PipelineState &rhs) const;

            bool operator()(const PipelineCacheKey &lhs, const PipelineCacheKey &rhs) const;
        };

        struct PipelineCacheEntry {
            vk::raii::DescriptorSetLayout descriptorSetLayout;
            vk::raii::PipelineLayout pipelineLayout;
            vk::raii::Pipeline pipeline;

            PipelineCacheEntry(vk::raii::DescriptorSetLayout&& descriptorSetLayout, vk::raii::PipelineLayout &&layout, vk::raii::Pipeline &&pipeline);
        };

        std::unordered_map<PipelineCacheKey, PipelineCacheEntry, PipelineStateHash, PipelineCacheEqual> pipelineCache;

      public:
        GraphicsPipelineCache(GPU &gpu);

        struct CompiledPipeline {
            vk::DescriptorSetLayout descriptorSetLayout;
            vk::PipelineLayout pipelineLayout;
            vk::Pipeline pipeline;

            CompiledPipeline(const PipelineCacheEntry &entry);
        };

        /**
         * @note All attachments in the PipelineState **must** be locked prior to calling this function
         * @note Shader specializiation constants are **not** supported and will result in UB
         * @note Input/Resolve attachments are **not** supported and using them with the supplied pipeline will result in UB
         */
        CompiledPipeline GetCompiledPipeline(const PipelineState& state, span<const vk::DescriptorSetLayoutBinding> layoutBindings, span<const vk::PushConstantRange> pushConstantRanges = {});
    };
}
