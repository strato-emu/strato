// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <tsl/robin_map.h>
#include <shader_compiler/frontend/ir/program.h>
#include <gpu/cache/graphics_pipeline_cache.h>
#include <gpu/interconnect/common/samplers.h>
#include <gpu/interconnect/common/textures.h>
#include "common.h"
#include "packed_pipeline_state.h"
#include "constant_buffers.h"

namespace skyline::gpu {
    class TextureView;
}

namespace skyline::gpu::interconnect::maxwell3d {
    class Pipeline {
      public:
        struct ShaderStage {
            vk::ShaderStageFlagBits stage;
            vk::ShaderModule module;
            Shader::Info info;

            /**
             * @return Whether the bindings for this stage match those of the input stage
             */
            bool BindingsEqual(const ShaderStage &other) const {
                return info.constant_buffer_descriptors == other.info.constant_buffer_descriptors &&
                    info.storage_buffers_descriptors == other.info.storage_buffers_descriptors &&
                    info.texture_buffer_descriptors == other.info.texture_buffer_descriptors &&
                    info.image_buffer_descriptors == other.info.image_buffer_descriptors &&
                    info.texture_descriptors == other.info.texture_descriptors &&
                    info.image_descriptors == other.info.image_descriptors;
            }
        };

        struct DescriptorInfo {
            std::vector<vk::DescriptorSetLayoutBinding> descriptorSetLayoutBindings;

            struct StageDescriptorInfo {
                u32 uniformBufferDescCount;
                u32 storageBufferDescCount;
                u32 uniformTexelBufferDescCount;
                u32 storageTexelBufferDescCount;
                u32 combinedImageSamplerDescCount;
                u32 storageImageDescCount;

                /**
                 * @brief Keeps track of all bindings that are dependent on a given constant buffer index to allow for quick binding
                 */
                struct ConstantBufferDescriptorUsages {
                    struct Usage {
                        u32 binding; //!< Vulkan binding index
                        u32 shaderDescIdx; //!< Index of the descriptor in the appropriate shader info member
                        u32 entirePipelineIdx; //!< Index of the image/storage buffer in the entire pipeline
                    };

                    boost::container::small_vector<Usage, 2> uniformBuffers;
                    boost::container::small_vector<Usage, 2> storageBuffers;
                    boost::container::small_vector<Usage, 2> combinedImageSamplers;
                    u32 totalBufferDescCount;
                    u32 totalImageDescCount;
                    u32 writeDescCount;
                };

                std::array<ConstantBufferDescriptorUsages, engine::ShaderStageConstantBufferCount> cbufUsages;
            };

            std::vector<vk::CopyDescriptorSet> copyDescs;
            std::array<StageDescriptorInfo, 5> stages;

            u32 totalStorageBufferCount;
            u32 totalCombinedImageSamplerCount;

            u32 totalWriteDescCount;
            u32 totalBufferDescCount;
            u32 totalTexelBufferDescCount;
            u32 totalImageDescCount;
        };

      private:
        std::vector<CachedMappedBufferView> storageBufferViews;
        u32 lastExecutionNumber{}; //!< The last execution number this pipeline was used at
        std::array<ShaderStage, engine::ShaderStageCount> shaderStages;
        DescriptorInfo descriptorInfo;

        std::array<Pipeline *, 4> transitionCache{};
        size_t transitionCacheNextIdx{};

        tsl::robin_map<Pipeline *, bool> bindingMatchCache; //!< Cache of which pipelines have bindings that match this pipeline

        void SyncCachedStorageBufferViews(u32 executionNumber);

      public:
        cache::GraphicsPipelineCache::CompiledPipeline compiledPipeline;
        size_t sampledImageCount{};

        PackedPipelineState sourcePackedState;

        Pipeline(InterconnectContext &ctx, Textures &textures, ConstantBufferSet &constantBuffers, const PackedPipelineState &packedState, const std::array<ShaderBinary, engine::PipelineCount> &shaderBinaries, span<TextureView *> colorAttachments, TextureView *depthAttachment);

        Pipeline *LookupNext(const PackedPipelineState &packedState);

        void AddTransition(Pipeline *next);

        bool CheckBindingMatch(Pipeline *other);

        u32 GetTotalSampledImageCount() const;

        /**
         * @brief Creates a descriptor set update from the current GPU state
         * @param sampledImages A span of size `GetTotalSampledImageCount()` in which texture view pointers for each sampled image will be written
         */
        DescriptorUpdateInfo *SyncDescriptors(InterconnectContext &ctx, ConstantBufferSet &constantBuffers, Samplers &samplers, Textures &textures, span<TextureView *> sampledImages);

        /**
         * @brief Creates a partial descriptor set update from the current GPU state for only the subset of descriptors changed by the quick bind constant buffer
         * @param sampledImages A span of size `GetTotalSampledImageCount()` in which texture view pointers for each sampled image will be written
         */
        DescriptorUpdateInfo *SyncDescriptorsQuickBind(InterconnectContext &ctx, ConstantBufferSet &constantBuffers, Samplers &samplers, Textures &textures, ConstantBuffers::QuickBind quickBind, span<TextureView *> sampledImages);
    };

    class PipelineManager {
      private:
        tsl::robin_map<PackedPipelineState, std::unique_ptr<Pipeline>, PackedPipelineStateHash> map;

      public:
        Pipeline *FindOrCreate(InterconnectContext &ctx, Textures &textures, ConstantBufferSet &constantBuffers, const PackedPipelineState &packedState, const std::array<ShaderBinary, engine::PipelineCount> &shaderBinaries, span<TextureView *> colorAttachments, TextureView *depthAttachment) {
            auto it{map.find(packedState)};
            if (it != map.end())
                return it->second.get();

            return map.emplace(packedState, std::make_unique<Pipeline>(ctx, textures, constantBuffers, packedState, shaderBinaries, colorAttachments, depthAttachment)).first->second.get();
        }
    };
}
