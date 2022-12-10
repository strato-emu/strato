// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "pipeline_state_accessor.h"
#include "pipeline_state_bundle.h"

namespace skyline::gpu::interconnect::maxwell3d {
    /**
     * @brief Implements the PipelineStateAccessor interface for pipelines loaded from a file
     */
    class FilePipelineStateAccessor : public PipelineStateAccessor {
      private:
        PipelineStateBundle &bundle;

      public:
        FilePipelineStateAccessor(PipelineStateBundle &bundle);

        Shader::TextureType GetTextureType(u32 index) const override;

        u32 GetConstantBufferValue(u32 shaderStage, u32 index, u32 offset) const override;

        ShaderBinary GetShaderBinary(u32 pipelineStage) const override;

        void MarkComplete() override;
    };
}
