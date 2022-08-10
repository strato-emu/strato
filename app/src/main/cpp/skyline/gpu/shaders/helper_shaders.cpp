// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <gpu/descriptor_allocator.h>
#include <gpu/texture/texture.h>
#include <gpu/cache/graphics_pipeline_cache.h>
#include <vfs/filesystem.h>
#include "helper_shaders.h"

namespace skyline::gpu {
    static vk::raii::ShaderModule CreateShaderModule(GPU &gpu, vfs::Backing &shaderBacking) {
        std::vector<u32> shaderBuf(shaderBacking.size / 4);

        if (shaderBacking.Read(span(shaderBuf)) != shaderBacking.size)
            throw exception("Failed to read shader");

        return gpu.vkDevice.createShaderModule(
            {
                .pCode = shaderBuf.data(),
                .codeSize = shaderBacking.size,
            }
        );
    }

    SimpleColourRTShader::SimpleColourRTShader(GPU &gpu, std::shared_ptr<vfs::Backing> vertexShader, std::shared_ptr<vfs::Backing> fragmentShader)
        : vertexShaderModule{CreateShaderModule(gpu, *vertexShader)},
          fragmentShaderModule{CreateShaderModule(gpu, *fragmentShader)},
          shaderStages{{
                           vk::PipelineShaderStageCreateInfo{
                               .stage = vk::ShaderStageFlagBits::eVertex,
                               .pName = "main",
                               .module = *vertexShaderModule
                           },
                           vk::PipelineShaderStageCreateInfo{
                               .stage = vk::ShaderStageFlagBits::eFragment,
                               .pName = "main",
                               .module = *fragmentShaderModule
                           }}
          } {}

    cache::GraphicsPipelineCache::CompiledPipeline SimpleColourRTShader::GetPipeline(GPU &gpu,
                                                                                     TextureView *colorAttachment,
                                                                                     span<const vk::DescriptorSetLayoutBinding> layoutBindings, span<const vk::PushConstantRange> pushConstantRanges) {
        constexpr static vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState{
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = false
        };

        constexpr static vk::PipelineTessellationStateCreateInfo tesselationState{
            .patchControlPoints = 0,
        };

        const static vk::StructureChain<vk::PipelineRasterizationStateCreateInfo, vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT> rasterizationState{
            {
                .depthClampEnable = false,
                .rasterizerDiscardEnable = false,
                .polygonMode = vk::PolygonMode::eFill,
                .lineWidth = 1.0f,
                .cullMode = vk::CullModeFlagBits::eNone,
                .frontFace = vk::FrontFace::eClockwise,
                .depthBiasEnable = false
            }, {
                .provokingVertexMode = vk::ProvokingVertexModeEXT::eFirstVertex
            }
        };

        constexpr static vk::PipelineMultisampleStateCreateInfo multisampleState{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = false,
            .minSampleShading = 1.0f,
            .alphaToCoverageEnable = false,
            .alphaToOneEnable = false
        };

        constexpr static vk::PipelineDepthStencilStateCreateInfo depthStencilState{
            .depthTestEnable = false,
            .depthWriteEnable = false,
            .depthBoundsTestEnable = false,
            .stencilTestEnable = false,
        };

        constexpr static vk::PipelineColorBlendAttachmentState attachmentState{
            .blendEnable = false,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
        };

        constexpr static vk::PipelineColorBlendStateCreateInfo blendState{
            .logicOpEnable = false,
            .attachmentCount = 1,
            .pAttachments = &attachmentState
        };

        vk::StructureChain<vk::PipelineVertexInputStateCreateInfo, vk::PipelineVertexInputDivisorStateCreateInfoEXT> vertexState{
            {
                .vertexAttributeDescriptionCount = 0,
                .vertexBindingDescriptionCount = 0
            }, {}
        };

        vertexState.unlink<vk::PipelineVertexInputDivisorStateCreateInfoEXT>();

        auto colourAttachmentDimensions{colorAttachment->texture->dimensions};

        vk::Viewport viewport{
            .height = static_cast<float>(colourAttachmentDimensions.height),
            .width = static_cast<float>(colourAttachmentDimensions.width),
            .x = 0.0f,
            .y = 0.0f,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vk::Rect2D scissor{
            .extent = colourAttachmentDimensions
        };

        vk::PipelineViewportStateCreateInfo viewportState{
            .pViewports = &viewport,
            .viewportCount = 1,
            .pScissors = &scissor,
            .scissorCount = 1
        };

        return gpu.graphicsPipelineCache.GetCompiledPipeline(cache::GraphicsPipelineCache::PipelineState{
            .shaderStages = shaderStages,
            .vertexState = vertexState,
            .inputAssemblyState = inputAssemblyState,
            .tessellationState = tesselationState,
            .viewportState = viewportState,
            .rasterizationState = rasterizationState,
            .multisampleState = multisampleState,
            .depthStencilState = depthStencilState,
            .colorBlendState = blendState,
            .colorAttachments = span<TextureView *>{colorAttachment},
            .depthStencilAttachment = nullptr,
        }, layoutBindings, pushConstantRanges);
    }

    namespace glsl {
        struct Vec2 {
            float x, y;
        };
    }

    namespace blit {
        struct VertexPushConstantLayout {
            glsl::Vec2 dstOriginClipSpace;
            glsl::Vec2 dstDimensionsClipSpace;
        };

        struct FragmentPushConstantLayout {
            glsl::Vec2 srcOriginUV;
            glsl::Vec2 dstSrcScaleFactor;
            float srcHeightRecip;
        };

        constexpr static std::array<vk::PushConstantRange, 2> PushConstantRanges{
            vk::PushConstantRange{
                .stageFlags = vk::ShaderStageFlagBits::eVertex,
                .size = sizeof(VertexPushConstantLayout),
                .offset = 0
            }, vk::PushConstantRange{
                .stageFlags = vk::ShaderStageFlagBits::eFragment,
                .size = sizeof(FragmentPushConstantLayout),
                .offset = sizeof(VertexPushConstantLayout)
            }
        };

        constexpr static vk::DescriptorSetLayoutBinding SamplerLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment
        };
    };

    BlitHelperShader::BlitHelperShader(GPU &gpu, std::shared_ptr<vfs::FileSystem> shaderFileSystem)
        : SimpleColourRTShader{gpu, shaderFileSystem->OpenFile("shaders/blit.vert.spv"), shaderFileSystem->OpenFile("shaders/blit.frag.spv")},
          bilinearSampler{gpu.vkDevice.createSampler(
              vk::SamplerCreateInfo{
                  .addressModeU = vk::SamplerAddressMode::eRepeat,
                  .addressModeV = vk::SamplerAddressMode::eRepeat,
                  .addressModeW = vk::SamplerAddressMode::eRepeat,
                  .anisotropyEnable = false,
                  .compareEnable = false,
                  .magFilter = vk::Filter::eLinear,
                  .minFilter = vk::Filter::eLinear
              })
          },
          nearestSampler{gpu.vkDevice.createSampler(
              vk::SamplerCreateInfo{
                  .addressModeU = vk::SamplerAddressMode::eRepeat,
                  .addressModeV = vk::SamplerAddressMode::eRepeat,
                  .addressModeW = vk::SamplerAddressMode::eRepeat,
                  .anisotropyEnable = false,
                  .compareEnable = false,
                  .magFilter = vk::Filter::eNearest,
                  .minFilter = vk::Filter::eNearest
              })
          } {}

    void BlitHelperShader::Blit(GPU &gpu, BlitRect srcRect, BlitRect dstRect,
                                vk::Extent2D srcImageDimensions, vk::Extent2D dstImageDimensions,
                                float dstSrcScaleFactorX, float dstSrcScaleFactorY,
                                bool bilinearFilter,
                                TextureView *srcImageView, TextureView *dstImageView,
                                std::function<void(std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32)> &&)> &&recordCb) {
        struct DrawState {
            blit::VertexPushConstantLayout vertexPushConstants;
            blit::FragmentPushConstantLayout fragmentPushConstants;
            DescriptorAllocator::ActiveDescriptorSet descriptorSet;
            cache::GraphicsPipelineCache::CompiledPipeline pipeline;

            DrawState(GPU &gpu,
                      blit::VertexPushConstantLayout vertexPushConstants, blit::FragmentPushConstantLayout fragmentPushConstants,
                      cache::GraphicsPipelineCache::CompiledPipeline pipeline)
                : vertexPushConstants{vertexPushConstants}, fragmentPushConstants{fragmentPushConstants},
                  descriptorSet{gpu.descriptor.AllocateSet(pipeline.descriptorSetLayout)},
                  pipeline{pipeline} {}
        };

        auto drawState{std::make_shared<DrawState>(
            gpu,
            blit::VertexPushConstantLayout{
                .dstOriginClipSpace = {(2.0f * dstRect.x) / dstImageDimensions.width - 1.0f, (2.0f * dstRect.y) / dstImageDimensions.height - 1.0f},
                .dstDimensionsClipSpace = {(2.0f * dstRect.width) / dstImageDimensions.width, (2.0f * dstRect.height) / dstImageDimensions.height}
            }, blit::FragmentPushConstantLayout{
                .srcOriginUV = {srcRect.x / srcImageDimensions.width, srcRect.y / srcImageDimensions.height},
                .dstSrcScaleFactor = {dstSrcScaleFactorX * (srcRect.width / srcImageDimensions.width), dstSrcScaleFactorY * (srcRect.height / srcImageDimensions.height)},
                .srcHeightRecip = 1.0f / srcImageDimensions.height
            },
            GetPipeline(gpu, dstImageView, {blit::SamplerLayoutBinding}, blit::PushConstantRanges))
        };

        vk::DescriptorImageInfo imageInfo{
            .imageLayout = vk::ImageLayout::eGeneral,
            .imageView = srcImageView->GetView(),
            .sampler = bilinearFilter ? *bilinearSampler : *nearestSampler
        };

        std::array<vk::WriteDescriptorSet, 1> writes{vk::WriteDescriptorSet{
            .dstBinding = 0,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .dstSet = *drawState->descriptorSet,
            .pImageInfo = &imageInfo
        }};

        gpu.vkDevice.updateDescriptorSets(writes, nullptr);

        recordCb([drawState = std::move(drawState)](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle, GPU &gpu, vk::RenderPass, u32) {
            cycle->AttachObject(drawState);
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, drawState->pipeline.pipeline);
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, drawState->pipeline.pipelineLayout, 0, *drawState->descriptorSet, nullptr);
            commandBuffer.pushConstants(drawState->pipeline.pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
                                        vk::ArrayProxy<const blit::VertexPushConstantLayout>{drawState->vertexPushConstants});
            commandBuffer.pushConstants(drawState->pipeline.pipelineLayout, vk::ShaderStageFlagBits::eFragment, sizeof(blit::VertexPushConstantLayout),
                                        vk::ArrayProxy<const blit::FragmentPushConstantLayout>{drawState->fragmentPushConstants});
            commandBuffer.draw(6, 1, 0, 0);
        });

    }

    HelperShaders::HelperShaders(GPU &gpu, std::shared_ptr<vfs::FileSystem> shaderFileSystem)
        : blitHelperShader(gpu, std::move(shaderFileSystem)) {}

}