// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <range/v3/algorithm.hpp>
#include "pipeline_state_bundle.h"

namespace skyline::gpu::interconnect {
    void PipelineStateBundle::PipelineStage::Reset() {
        binary.clear();
        binaryBaseOffset = 0;
    }

    PipelineStateBundle::PipelineStateBundle() {}

    void PipelineStateBundle::Reset(span<const u8> newKey) {
        ranges::for_each(pipelineStages, [](auto &stage) { stage.Reset(); });
        key.resize(newKey.size());
        span(key).copy_from(newKey);
        textureTypes.clear();
        constantBufferValues.clear();
    }

    void PipelineStateBundle::SetShaderBinary(u32 pipelineStage, ShaderBinary bin) {
        if (pipelineStages.size() <= pipelineStage)
            pipelineStages.resize(pipelineStage + 1);
        auto &stageInfo{pipelineStages[pipelineStage]};
        stageInfo.binary.resize(bin.binary.size());
        span(stageInfo.binary).copy_from(bin.binary);
        stageInfo.binaryBaseOffset = bin.baseOffset;
    }

    void PipelineStateBundle::AddTextureType(u32 index, Shader::TextureType type) {
        textureTypes.emplace_back(index, type);
    }

    void PipelineStateBundle::AddConstantBufferValue(u32 shaderStage, u32 index, u32 offset, u32 value) {
        constantBufferValues.push_back({shaderStage, index, offset, value});
    }

    span<u8> PipelineStateBundle::GetKey() {
        return span(key);
    }

    ShaderBinary PipelineStateBundle::GetShaderBinary(u32 pipelineStage) {
        auto &stageInfo{pipelineStages[pipelineStage]};
        return {stageInfo.binary, stageInfo.binaryBaseOffset};
    }

    Shader::TextureType PipelineStateBundle::LookupTextureType(u32 offset) {
        auto it{ranges::find_if(textureTypes, [offset](auto &pair) { return pair.first == offset; })};
        if (it == textureTypes.end())
            throw exception("Failed to find texture type for offset: 0x{:X}", offset);

        return it->second;
    }

    u32 PipelineStateBundle::LookupConstantBufferValue(u32 shaderStage, u32 index, u32 offset) {
        auto it{ranges::find_if(constantBufferValues, [index, offset, shaderStage](auto &val) { return  val.index == index && val.offset == offset && val.shaderStage == shaderStage; })};
        if (it == constantBufferValues.end())
            throw exception("Failed to find constant buffer value for offset: 0x{:X}", offset);

        return it->value;
    }
}
