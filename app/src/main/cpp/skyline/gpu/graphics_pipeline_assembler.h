// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <future>
#include <BS_thread_pool.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace skyline::gpu {
    class TextureView;
}

namespace skyline::gpu {
    /**
     * @brief Wrapper for Vulkan pipelines to allow for asynchronous compilation
     */
    class GraphicsPipelineAssembler {
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
            const vk::PipelineDynamicStateCreateInfo &dynamicState;

            span<vk::Format> colorFormats; //!< All color attachment formats in the subpass of this pipeline
            vk::Format depthStencilFormat; //!< The depth attachment format in the subpass of this pipeline, 'Undefined' if there is no depth attachment
            vk::SampleCountFlagBits sampleCount; //!< The sample count of the subpass of this pipeline
            bool destroyShaderModules; //!< Whether the shader modules should be destroyed after the pipeline is compiled

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
        vk::raii::PipelineCache vkPipelineCache; //!< A Vulkan Pipeline Cache which stores all unique graphics pipelines
        BS::thread_pool pool;
        std::string pipelineCacheDir;

        /**
         * @brief All unique metadata in a single attachment for a compatible render pass according to Render Pass Compatibility clause in the Vulkan specification
         * @url https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#renderpass-compatibility
         * @url https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkAttachmentDescription.html
         */
        struct AttachmentMetadata {
            vk::Format format;

            constexpr AttachmentMetadata(vk::Format format, vk::SampleCountFlagBits sampleCount) : format(format) {}

            bool operator==(const AttachmentMetadata &rhs) const = default;
        };

        struct PipelineDescription {
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
            std::vector<vk::DynamicState> dynamicStates;
            vk::PipelineDynamicStateCreateInfo dynamicState;
            std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments;

            std::vector<vk::Format> colorFormats;
            vk::Format depthStencilFormat;
            vk::SampleCountFlagBits sampleCount;
            bool destroyShaderModules;

            PipelineDescription(const PipelineState& state);

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

        std::mutex mutex; //!< Protects access to `compilePendingDescs`
        std::list<PipelineDescription> compilePendingDescs; //!< List of pipeline descriptions that are pending compilation

        /**
         * @brief Synchronously compiles a pipeline with the state from the given description
         */
        vk::raii::Pipeline AssemblePipeline(std::list<PipelineDescription>::iterator pipelineDescIt, vk::PipelineLayout pipelineLayout);

      public:
        GraphicsPipelineAssembler(GPU &gpu, std::string_view pipelineCacheDir);

        struct CompiledPipeline {
            vk::raii::DescriptorSetLayout descriptorSetLayout;
            vk::raii::PipelineLayout pipelineLayout;
            std::shared_future<vk::raii::Pipeline> pipeline;

            CompiledPipeline() : descriptorSetLayout{nullptr}, pipelineLayout{nullptr} {};

            CompiledPipeline(vk::raii::DescriptorSetLayout descriptorSetLayout,
                             vk::raii::PipelineLayout pipelineLayout,
                             std::shared_future<vk::raii::Pipeline> pipeline)
                : descriptorSetLayout{std::move(descriptorSetLayout)},
                  pipelineLayout{std::move(pipelineLayout)},
                  pipeline{std::move(pipeline)} {};
        };

        /**
         * @note All attachments in the PipelineState **must** be locked prior to calling this function
         * @note Shader specializiation constants are **not** supported and will result in UB
         * @note Input/Resolve attachments are **not** supported and using them with the supplied pipeline will result in UB
         */
        CompiledPipeline AssemblePipelineAsync(const PipelineState &state, span<const vk::DescriptorSetLayoutBinding> layoutBindings, span<const vk::PushConstantRange> pushConstantRanges = {}, bool noPushDescriptors = false);

        /**
         * @brief Waits until the pipeline compilation thread pool is idle and all pipelines have been compiled
         */
        void WaitIdle();

        /**
         * @brief Saves the current Vulkan pipeline cache to the filesystem
         */
        void SavePipelineCache();
    };
}
