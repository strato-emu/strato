// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <tsl/robin_map.h>
#include <shader_compiler/frontend/ir/program.h>
#include <gpu/interconnect/common/samplers.h>
#include <gpu/interconnect/common/textures.h>
#include "packed_pipeline_state.h"
#include "constant_buffers.h"


namespace skyline::gpu {
    class TextureView;
}

namespace skyline::gpu::interconnect::kepler_compute {
    class Pipeline {
      public:
        struct ShaderStage {
            vk::ShaderModule module;
            Shader::Info info;
        };

        struct DescriptorInfo {
            std::vector<vk::DescriptorSetLayoutBinding> descriptorSetLayoutBindings;

            u32 totalWriteDescCount;
            u32 totalBufferDescCount;
            u32 totalTexelBufferDescCount;
            u32 totalImageDescCount;
        };

        struct CompiledPipeline {
            vk::raii::DescriptorSetLayout descriptorSetLayout;
            vk::raii::PipelineLayout pipelineLayout;
            vk::raii::Pipeline pipeline;
        };

      private:
        ShaderStage shaderStage;
        DescriptorInfo descriptorInfo;
        std::vector<CachedMappedBufferView> storageBufferViews;
        u32 lastExecutionNumber{}; //!< The last execution number this pipeline was used at

        void SyncCachedStorageBufferViews(u32 executionNumber);

      public:
        CompiledPipeline compiledPipeline;

        PackedPipelineState sourcePackedState;

        Pipeline(InterconnectContext &ctx, Textures &textures, ConstantBufferSet &constantBuffers, const PackedPipelineState &packedState, const ShaderBinary &shaderBinary);

        /**
         * @brief Creates a descriptor set update from the current GPU state
         */
        DescriptorUpdateInfo *SyncDescriptors(InterconnectContext &ctx, ConstantBufferSet &constantBuffers, Samplers &samplers, Textures &textures);
    };

    class PipelineManager {
      private:
        tsl::robin_map<PackedPipelineState, std::unique_ptr<Pipeline>, util::ObjectHash<PackedPipelineState>> map;

      public:
        Pipeline *FindOrCreate(InterconnectContext &ctx, Textures &textures, ConstantBufferSet &constantBuffers, const PackedPipelineState &packedState, const ShaderBinary &shaderBinary) {
            auto it{map.find(packedState)};
            if (it != map.end())
                return it->second.get();

            return map.emplace(packedState, std::make_unique<Pipeline>(ctx, textures, constantBuffers, packedState, shaderBinary)).first->second.get();
        }
    };
}
