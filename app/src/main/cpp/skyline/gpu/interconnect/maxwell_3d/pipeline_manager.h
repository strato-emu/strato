// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <tsl/robin_map.h>
#include <shader_compiler/frontend/ir/program.h>
#include <gpu/cache/graphics_pipeline_cache.h>
#include "common.h"
#include "packed_pipeline_state.h"
#include "constant_buffers.h"

namespace skyline::gpu {
    class TextureView;
}

namespace skyline::gpu::interconnect::maxwell3d {
    struct ShaderBinary {
        span<u8> binary;
        u32 baseOffset;
    };

    class Pipeline {
      public:
        struct ShaderStage {
            vk::ShaderStageFlagBits stage;
            vk::ShaderModule module;
            Shader::Info info;
        };

        struct DescriptorInfo {
            std::vector<vk::DescriptorSetLayoutBinding> descriptorSetLayoutBindings;
            u32 writeDescCount{};
            u32 uniformBufferDescCount{};
            u32 storageBufferDescCount{};
            u32 totalBufferDescCount{};
            u32 uniformTexelBufferDescCount{};
            u32 storageTexelBufferDescCount{};
            u32 totalTexelBufferDescCount{};
            u32 combinedImageSamplerDescCount{};
            u32 storageImageDescCount{};
            u32 totalImageDescCount{};
            u32 totalElemCount{};

            /**
             * @brief Keeps track of all bindings that are dependent on a given constant buffer index to allow for quick binding
             */
            struct ConstantBufferDescriptorUsages {
                struct Usage {
                    u32 binding; //!< Vulkan binding index
                    u32 shaderDescIdx; //!< Index of the descriptor in the appropriate shader info member
                };

                boost::container::small_vector<Usage, 2> uniformBuffers;
                boost::container::small_vector<Usage, 2> storageBuffers;
                u32 totalBufferDescCount{};
                u32 writeDescCount{};
            };

            std::array<std::array<ConstantBufferDescriptorUsages, engine::ShaderStageConstantBufferCount>, engine::ShaderStageCount> cbufUsages{};
        };

      private:
        std::vector<CachedMappedBufferView> storageBufferViews;
        std::array<ShaderStage, engine::ShaderStageCount> shaderStages;
        DescriptorInfo descriptorInfo;
        cache::GraphicsPipelineCache::CompiledPipeline compiledPipeline;

        std::array<Pipeline *, 4> transitionCache{};
        size_t transitionCacheNextIdx{};

      public:
        PackedPipelineState sourcePackedState;

        Pipeline(InterconnectContext &ctx, const PackedPipelineState &packedState, const std::array<ShaderBinary, engine::PipelineCount> &shaderBinaries, span<TextureView *> colorAttachments, TextureView *depthAttachment);

        Pipeline *LookupNext(const PackedPipelineState &packedState);

        void AddTransition(Pipeline *next);

        void SyncDescriptors(InterconnectContext &ctx, ConstantBufferSet &constantBuffers);

        void SyncDescriptorsQuickBind(InterconnectContext &ctx, ConstantBufferSet &constantBuffers, ConstantBuffers::QuickBind quickBind);
    };

    class PipelineManager {
      private:
        tsl::robin_map<PackedPipelineState, std::unique_ptr<Pipeline>, util::ObjectHash<PackedPipelineState>> map;

      public:
        Pipeline *FindOrCreate(InterconnectContext &ctx, const PackedPipelineState &packedState, const std::array<ShaderBinary, engine::PipelineCount> &shaderBinaries, span<TextureView *> colorAttachments, TextureView *depthAttachment) {
            auto it{map.find(packedState)};
            if (it != map.end())
                return it->second.get();

            return map.emplace(packedState, std::make_unique<Pipeline>(ctx, packedState, shaderBinaries, colorAttachments, depthAttachment)).first->second.get();
        }
    };
}
