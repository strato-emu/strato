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
        textureTypes.push_back({index, type});
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

    Shader::TextureType PipelineStateBundle::LookupTextureType(u32 index) {
        auto it{ranges::find_if(textureTypes, [index](const auto &entry) { return entry.index == index; })};
        if (it == textureTypes.end())
            throw exception("Failed to find texture type for index: 0x{:X}", index);

        return it->type;
    }

    u32 PipelineStateBundle::LookupConstantBufferValue(u32 shaderStage, u32 index, u32 offset) {
        auto it{ranges::find_if(constantBufferValues, [index, offset, shaderStage](const auto &val) { return  val.index == index && val.offset == offset && val.shaderStage == shaderStage; })};
        if (it == constantBufferValues.end())
            throw exception("Failed to find constant buffer value for offset: 0x{:X}", offset);

        return it->value;
    }

    static constexpr u32 MaxSerialisedBundleSize{1 << 20}; ///< The maximum size of a serialised bundle (1 MiB)

    /*  Bundle header format pseudocode:
        u64 hash
        u32 bundleSize
        u32 keySize;
        u32 constantBufferValueCount
        u32 textureTypeCount
        u32 pipelineStageCount
        u8 key[keySize];

        struct ConstantBufferValue {
            u32 shaderStage;
            u32 index;
            u32 offset;
            u32 value;
        } constantBufferValues[constantBufferValueCount];

        struct TextureType {
            u32 index;
            u32 (Shader::TextureType) type;
        } textureType[textureTypeCount];

        struct PipelineStage {
            u32 binaryBaseOffset
            u32 binarySize
            u8 binary[binarySize]
        } pipelineStages[pipelineStageCount];
    */

    struct BundleDataHeader {
        u32 keySize;
        u32 constantBufferValueCount;
        u32 textureTypeCount;
        u32 pipelineStageCount;
    };

    struct PipelineBinaryDataHeader {
        u32 binaryBaseOffset;
        u32 binarySize;
    };

    bool PipelineStateBundle::Deserialise(std::ifstream &stream) {
        if (stream.peek() == EOF)
            return false;

        u64 hash{};
        stream.read(reinterpret_cast<char *>(&hash), sizeof(hash));

        u32 bundleSize{};
        stream.read(reinterpret_cast<char *>(&bundleSize), sizeof(bundleSize));
        if (bundleSize > MaxSerialisedBundleSize)
            throw exception("Pipeline state bundle is too large: 0x{:X}", bundleSize);

        fileBuffer.resize(static_cast<size_t>(bundleSize));
        stream.read(reinterpret_cast<char *>(fileBuffer.data()), static_cast<std::streamsize>(bundleSize));

        if (XXH64(fileBuffer.data(), bundleSize, 0) != hash)
            throw exception("Pipeline state bundle hash mismatch");

        auto data{span(fileBuffer)};
        const auto &header{data.as<BundleDataHeader>()};
        size_t offset{sizeof(BundleDataHeader)};

        Reset(data.subspan(offset, header.keySize));
        offset += header.keySize;

        auto readConstantBufferValues{data.subspan(offset, header.constantBufferValueCount * sizeof(ConstantBufferValue)).cast<ConstantBufferValue>()};
        textureTypes.reserve(header.textureTypeCount);
        constantBufferValues.insert(constantBufferValues.end(), readConstantBufferValues.begin(), readConstantBufferValues.end());
        offset += header.constantBufferValueCount * sizeof(ConstantBufferValue);

        auto readTextureTypes{data.subspan(offset, header.textureTypeCount * sizeof(TextureTypeEntry)).cast<TextureTypeEntry>()};
        textureTypes.reserve(header.textureTypeCount);
        textureTypes.insert(textureTypes.end(), readTextureTypes.begin(), readTextureTypes.end());
        offset += header.textureTypeCount * sizeof(TextureTypeEntry);

        pipelineStages.resize(header.pipelineStageCount);
        for (u32 i{}; i < header.pipelineStageCount; i++) {
            const auto &pipelineHeader{data.subspan(offset).as<PipelineBinaryDataHeader>()};
            offset += sizeof(PipelineBinaryDataHeader);

            pipelineStages[i].binaryBaseOffset = pipelineHeader.binaryBaseOffset;
            pipelineStages[i].binary.resize(pipelineHeader.binarySize);
            span(pipelineStages[i].binary).copy_from(data.subspan(offset, pipelineHeader.binarySize));
            offset += pipelineHeader.binarySize;
        }

        return true;
    }

    void PipelineStateBundle::Serialise(std::ofstream &stream) {
        u32 bundleSize{static_cast<u32>(sizeof(BundleDataHeader) +
                                        key.size() +
                                        constantBufferValues.size() * sizeof(ConstantBufferValue) +
                                        textureTypes.size() * sizeof(TextureTypeEntry) +
                                        std::accumulate(pipelineStages.begin(), pipelineStages.end(), 0UL, [](size_t acc, const auto &stage) {
                                            return acc + sizeof(PipelineBinaryDataHeader) + stage.binary.size();
                                        }))};

        fileBuffer.resize(bundleSize);

        auto data{span(fileBuffer)};
        auto &header{data.as<BundleDataHeader>()};
        size_t offset{sizeof(BundleDataHeader)};

        header.keySize = static_cast<u32>(key.size());
        header.constantBufferValueCount = static_cast<u32>(constantBufferValues.size());
        header.textureTypeCount = static_cast<u32>(textureTypes.size());
        header.pipelineStageCount = static_cast<u32>(pipelineStages.size());

        data.subspan(offset, header.keySize).copy_from(key);
        offset += header.keySize;

        data.subspan(offset, header.constantBufferValueCount * sizeof(ConstantBufferValue)).copy_from(constantBufferValues);
        offset += header.constantBufferValueCount * sizeof(ConstantBufferValue);

        data.subspan(offset, header.textureTypeCount * sizeof(TextureTypeEntry)).copy_from(textureTypes);
        offset += header.textureTypeCount * sizeof(TextureTypeEntry);

        for (const auto &stage : pipelineStages) {
            auto &pipelineHeader{data.subspan(offset).as<PipelineBinaryDataHeader>()};
            offset += sizeof(PipelineBinaryDataHeader);

            pipelineHeader.binaryBaseOffset = stage.binaryBaseOffset;
            pipelineHeader.binarySize = static_cast<u32>(stage.binary.size());

            data.subspan(offset, pipelineHeader.binarySize).copy_from(stage.binary);
            offset += pipelineHeader.binarySize;
        }

        u64 hash{XXH64(fileBuffer.data(), bundleSize, 0)};
        stream.write(reinterpret_cast<const char *>(&hash), sizeof(hash));
        stream.write(reinterpret_cast<const char *>(&bundleSize), sizeof(bundleSize));
        stream.write(reinterpret_cast<const char *>(fileBuffer.data()), static_cast<std::streamsize>(bundleSize));
    }
}
