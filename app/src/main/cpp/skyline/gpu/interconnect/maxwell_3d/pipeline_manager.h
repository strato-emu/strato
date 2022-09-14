// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <tsl/robin_map.h>
#include <shader_compiler/frontend/ir/program.h>
#include <gpu/cache/graphics_pipeline_cache.h>
#include "common.h"
#include "packed_pipeline_state.h"

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

      private:
        std::array<ShaderStage, engine::ShaderStageCount> shaderStages;
        std::vector<vk::DescriptorSetLayoutBinding> descriptorSetLayoutBindings;
        cache::GraphicsPipelineCache::CompiledPipeline compiledPipeline;

        std::array<Pipeline *, 4> transitionCache{};
        size_t transitionCacheNextIdx{};

      public:
        PackedPipelineState sourcePackedState;

        Pipeline(InterconnectContext &ctx, const PackedPipelineState &packedState, const std::array<ShaderBinary, engine::PipelineCount> &shaderBinaries, span<TextureView *> colorAttachments, TextureView *depthAttachment);

        Pipeline *LookupNext(const PackedPipelineState &packedState);

        void AddTransition(Pipeline *next);
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
