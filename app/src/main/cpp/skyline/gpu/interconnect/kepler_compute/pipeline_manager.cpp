// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu/texture/texture.h>
#include <gpu/interconnect/command_executor.h>
#include <gpu/interconnect/common/pipeline.inc>
#include <gpu/shader_manager.h>
#include <gpu.h>
#include "pipeline_manager.h"

namespace skyline::gpu::interconnect::kepler_compute {
    static Pipeline::ShaderStage MakePipelineShader(InterconnectContext &ctx, Textures &textures, ConstantBufferSet &constantBuffers, const PackedPipelineState &packedState, const ShaderBinary &shaderBinary) {
        ctx.gpu.shader->ResetPools();

        auto program{ctx.gpu.shader->ParseComputeShader(
            packedState.shaderHash, shaderBinary.binary, shaderBinary.baseOffset,
            packedState.bindlessTextureConstantBufferSlotSelect,
            packedState.localMemorySize, packedState.sharedMemorySize,
            packedState.dimensions,
            [&](u32 index, u32 offset) {
                return constantBuffers[index].Read<int>(ctx.executor, offset);
            }, [&](u32 index) {
                return textures.GetTextureType(ctx, BindlessHandle{ .raw = index }.textureIndex);
            })};

        Shader::Backend::Bindings bindings{};

        return {ctx.gpu.shader->CompileShader({}, program, bindings, packedState.shaderHash), program.info};
    }

    static Pipeline::DescriptorInfo MakePipelineDescriptorInfo(const Pipeline::ShaderStage &stage) {
        Pipeline::DescriptorInfo descriptorInfo{};
        u32 bindingIndex{};

        auto pushBindings{[&](vk::DescriptorType type, const auto &descs, u32 &count) {
            descriptorInfo.totalWriteDescCount += descs.size();

            for (u32 descIdx{}; descIdx < descs.size(); descIdx++) {
                const auto &desc{descs[descIdx]};
                count += desc.count;

                descriptorInfo.descriptorSetLayoutBindings.push_back(vk::DescriptorSetLayoutBinding{
                    .binding = bindingIndex++,
                    .descriptorType = type,
                    .descriptorCount = desc.count,
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
                });
            }
        }};

        pushBindings(vk::DescriptorType::eUniformBuffer, stage.info.constant_buffer_descriptors, descriptorInfo.totalBufferDescCount);
        pushBindings(vk::DescriptorType::eStorageBuffer, stage.info.storage_buffers_descriptors, descriptorInfo.totalBufferDescCount);

        pushBindings(vk::DescriptorType::eUniformTexelBuffer, stage.info.texture_buffer_descriptors, descriptorInfo.totalTexelBufferDescCount);
        pushBindings(vk::DescriptorType::eStorageTexelBuffer, stage.info.image_buffer_descriptors, descriptorInfo.totalTexelBufferDescCount);
        if (descriptorInfo.totalTexelBufferDescCount > 0)
            Logger::Warn("Image buffer descriptors are not supported");

        pushBindings(vk::DescriptorType::eCombinedImageSampler, stage.info.texture_descriptors, descriptorInfo.totalImageDescCount);
        pushBindings(vk::DescriptorType::eStorageImage, stage.info.image_descriptors, descriptorInfo.totalImageDescCount);
        if (stage.info.image_descriptors.size() > 0)
            Logger::Warn("Image descriptors are not supported");

        return descriptorInfo;
    }

    static Pipeline::CompiledPipeline MakeCompiledPipeline(InterconnectContext &ctx,
                                                                               const PackedPipelineState &packedState,
                                                                               const Pipeline::ShaderStage &shaderStage,
                                                                               span<vk::DescriptorSetLayoutBinding> layoutBindings) {
        vk::raii::DescriptorSetLayout descriptorSetLayout{ctx.gpu.vkDevice, vk::DescriptorSetLayoutCreateInfo{
            .flags = vk::DescriptorSetLayoutCreateFlags{ctx.gpu.traits.supportsPushDescriptors ? vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR : vk::DescriptorSetLayoutCreateFlags{}},
            .pBindings = layoutBindings.data(),
            .bindingCount = static_cast<u32>(layoutBindings.size()),
        }};

        vk::raii::PipelineLayout pipelineLayout{ctx.gpu.vkDevice, vk::PipelineLayoutCreateInfo{
            .pSetLayouts = &*descriptorSetLayout,
            .setLayoutCount = 1,
        }};

        vk::PipelineShaderStageCreateInfo shaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eCompute,
            .module = &*shaderStage.module,
            .pName = "main"
        };

        vk::ComputePipelineCreateInfo pipelineInfo{
            .stage = shaderStageInfo,
            .layout = *pipelineLayout,
        };


        if (ctx.gpu.traits.quirks.brokenMultithreadedPipelineCompilation)
            ctx.gpu.graphicsPipelineAssembler->WaitIdle();

        vk::raii::Pipeline pipeline{ctx.gpu.vkDevice, nullptr, pipelineInfo};

        return Pipeline::CompiledPipeline{
            .pipeline = std::move(pipeline),
            .pipelineLayout = std::move(pipelineLayout),
            .descriptorSetLayout = std::move(descriptorSetLayout),
        };
    }

    Pipeline::Pipeline(InterconnectContext &ctx, Textures &textures, ConstantBufferSet &constantBuffers, const PackedPipelineState &packedState, const ShaderBinary &shaderBinary)
        : shaderStage{MakePipelineShader(ctx, textures, constantBuffers, packedState, shaderBinary)},
          descriptorInfo{MakePipelineDescriptorInfo(shaderStage)},
          compiledPipeline{MakeCompiledPipeline(ctx, packedState, shaderStage, descriptorInfo.descriptorSetLayoutBindings)},
          sourcePackedState{packedState} {
        storageBufferViews.resize(shaderStage.info.storage_buffers_descriptors.size());
    }

    void Pipeline::SyncCachedStorageBufferViews(ContextTag executionTag) {
        if (lastExecutionTag != executionTag) {
            for (auto &view : storageBufferViews)
                view.PurgeCaches();

            lastExecutionTag = executionTag;
        }
    }

    DescriptorUpdateInfo *Pipeline::SyncDescriptors(InterconnectContext &ctx, ConstantBufferSet &constantBuffers, Samplers &samplers, Textures &textures, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask) {
        SyncCachedStorageBufferViews(ctx.executor.executionTag);

        u32 writeIdx{};
        auto writes{ctx.executor.allocator->AllocateUntracked<vk::WriteDescriptorSet>(descriptorInfo.totalWriteDescCount)};

        u32 bufferIdx{};
        auto bufferDescs{ctx.executor.allocator->AllocateUntracked<vk::DescriptorBufferInfo>(descriptorInfo.totalBufferDescCount)};
        auto bufferDescDynamicBindings{ctx.executor.allocator->AllocateUntracked<DynamicBufferBinding>(descriptorInfo.totalBufferDescCount)};
        u32 imageIdx{};
        auto imageDescs{ctx.executor.allocator->AllocateUntracked<vk::DescriptorImageInfo>(descriptorInfo.totalImageDescCount)};

        u32 storageBufferIdx{};
        u32 bindingIdx{};

        /**
         * @brief Adds descriptor writes for a single Vulkan descriptor type that uses buffer descriptors
         * @param count Total number of descriptors to write, including array elements
         */
        auto writeBufferDescs{[&](vk::DescriptorType type, const auto &descs, auto getBufferCb) {
            if (!descs.empty()) {
                // The underlying buffer bindings will be resolved from the dynamic ones during recording
                for (const auto &desc : descs) {
                    writes[writeIdx++] = {
                        .dstBinding = bindingIdx++,
                        .descriptorCount = desc.count,
                        .descriptorType = type,
                        .pBufferInfo = &bufferDescs[bufferIdx],
                    };

                    for (u32 arrayIdx{}; arrayIdx < desc.count; arrayIdx++)
                        bufferDescDynamicBindings[bufferIdx++] = getBufferCb(desc, arrayIdx);
                }
            }
        }};

        auto writeImageDescs{[&](vk::DescriptorType type, const auto &descs, auto getTextureCb) {
            if (!descs.empty()) {
                for (const auto &desc : descs) {
                    writes[writeIdx++] = {
                        .dstBinding = bindingIdx++,
                        .descriptorCount = desc.count,
                        .descriptorType = type,
                        .pImageInfo = &imageDescs[imageIdx],
                    };

                    for (u32 arrayIdx{}; arrayIdx < desc.count; arrayIdx++)
                        imageDescs[imageIdx++] = getTextureCb(desc, arrayIdx);
                }
            }
        }};

        writeBufferDescs(vk::DescriptorType::eUniformBuffer, shaderStage.info.constant_buffer_descriptors,
                         [&](const Shader::ConstantBufferDescriptor &desc, size_t arrayIdx) {
                             size_t cbufIdx{desc.index + arrayIdx};
                             return GetConstantBufferBinding(ctx, shaderStage.info.constant_buffer_used_sizes,
                                                             constantBuffers[cbufIdx].view, cbufIdx,
                                                             vk::PipelineStageFlagBits::eComputeShader,
                                                             srcStageMask, dstStageMask);
                         });

        writeBufferDescs(vk::DescriptorType::eStorageBuffer, shaderStage.info.storage_buffers_descriptors,
                         [&](const Shader::StorageBufferDescriptor &desc, size_t arrayIdx) {
                             auto binding{GetStorageBufferBinding(ctx, desc, constantBuffers[desc.cbuf_index],
                                                                  storageBufferViews[storageBufferIdx],
                                                                  vk::PipelineStageFlagBits::eComputeShader,
                                                                  srcStageMask, dstStageMask)};
                             storageBufferIdx += arrayIdx ? 0 : 1;
                             return binding;
                         });

        writeImageDescs(vk::DescriptorType::eCombinedImageSampler, shaderStage.info.texture_descriptors,
                        [&](const Shader::TextureDescriptor &desc, size_t arrayIdx) {
                            BindlessHandle handle{ReadBindlessHandle(ctx, constantBuffers, desc, arrayIdx)};
                            auto binding{GetTextureBinding(ctx, desc,
                                                           samplers, textures, handle,
                                                           vk::PipelineStageFlagBits::eComputeShader,
                                                           srcStageMask, dstStageMask)};
                            return binding.first;
                        });

        // Since we don't implement all descriptor types the number of writes might not match what's expected
        if (!writeIdx)
            return nullptr;

        return ctx.executor.allocator->EmplaceUntracked<DescriptorUpdateInfo>(DescriptorUpdateInfo{
            .writes = writes.first(writeIdx),
            .bufferDescs = bufferDescs.first(bufferIdx),
            .bufferDescDynamicBindings = bufferDescDynamicBindings.first(bufferIdx),
            .pipelineLayout = *compiledPipeline.pipelineLayout,
            .descriptorSetLayout = *compiledPipeline.descriptorSetLayout,
            .bindPoint = vk::PipelineBindPoint::eCompute,
            .descriptorSetIndex = 0,
        });
    }
}

