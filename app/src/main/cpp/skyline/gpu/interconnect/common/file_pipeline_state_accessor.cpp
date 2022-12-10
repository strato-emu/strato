// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "file_pipeline_state_accessor.h"

namespace skyline::gpu::interconnect::maxwell3d {
    FilePipelineStateAccessor::FilePipelineStateAccessor(PipelineStateBundle &bundle) : bundle{bundle} {}

    Shader::TextureType FilePipelineStateAccessor::GetTextureType(u32 index) const {
        return bundle.LookupTextureType(index);
    }

    u32 FilePipelineStateAccessor::GetConstantBufferValue(u32 shaderStage, u32 index, u32 offset) const {
        return bundle.LookupConstantBufferValue(shaderStage, index, offset);
    }

    ShaderBinary FilePipelineStateAccessor::GetShaderBinary(u32 pipelineStage) const {
        return bundle.GetShaderBinary(pipelineStage);
    }

    void FilePipelineStateAccessor::MarkComplete() {}
}
