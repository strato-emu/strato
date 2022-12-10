// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <gpu/interconnect/common/pipeline_state_accessor.h>
#include <gpu/interconnect/common/pipeline_state_bundle.h>
#include <gpu/interconnect/common/textures.h>
#include "constant_buffers.h"


namespace skyline::gpu::interconnect::maxwell3d {
    /**
     * @brief Implements the PipelineStateAccessor interface for pipelines created at emulator runtime
     */
    class RuntimeGraphicsPipelineStateAccessor : public PipelineStateAccessor {
      private:
        std::unique_ptr<PipelineStateBundle> bundle;
        InterconnectContext &ctx;
        Textures &textures;
        ConstantBufferSet &constantBuffers;
        std::array<ShaderBinary, engine::PipelineCount> shaderBinaries;

      public:
        RuntimeGraphicsPipelineStateAccessor(std::unique_ptr<PipelineStateBundle> bundle,
                                             InterconnectContext &ctx,
                                             Textures &textures, ConstantBufferSet &constantBuffers,
                                             const std::array<ShaderBinary, engine::PipelineCount> &shaderBinaries);

        Shader::TextureType GetTextureType(u32 index) const override;

        u32 GetConstantBufferValue(u32 shaderStage, u32 index, u32 offset) const override;

        ShaderBinary GetShaderBinary(u32 pipelineStage) const override;

        void MarkComplete() override;
    };
}
