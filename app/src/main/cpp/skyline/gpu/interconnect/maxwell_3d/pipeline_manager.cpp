// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu/texture/texture.h>
#include <gpu/shader_manager.h>
#include <gpu.h>
#include <gpu/interconnect/command_executor.h>
#include <vulkan/vulkan_handles.hpp>
#include "gpu/cache/graphics_pipeline_cache.h"
#include "pipeline_manager.h"
#include "soc/gm20b/engines/maxwell/types.h"

namespace skyline::gpu::interconnect::maxwell3d {
    static constexpr Shader::Stage ConvertCompilerShaderStage(engine::Pipeline::Shader::Type stage) {
        switch (stage) {
            case engine::Pipeline::Shader::Type::VertexCullBeforeFetch:
                return Shader::Stage::VertexA;
            case engine::Pipeline::Shader::Type::Vertex:
                return Shader::Stage::VertexB;
            case engine::Pipeline::Shader::Type::TessellationInit:
                return Shader::Stage::TessellationControl;
            case engine::Pipeline::Shader::Type::Tessellation:
                return Shader::Stage::TessellationEval;
            case engine::Pipeline::Shader::Type::Geometry:
                return Shader::Stage::Geometry;
            case engine::Pipeline::Shader::Type::Pixel:
                return Shader::Stage::Fragment;
            default:
                throw exception("Invalid shader stage: {}", stage);
        }
    }

    static vk::ShaderStageFlagBits ConvertVkShaderStage(engine::Pipeline::Shader::Type stage) {
        switch (stage) {
            case engine::Pipeline::Shader::Type::VertexCullBeforeFetch:
            case engine::Pipeline::Shader::Type::Vertex:
                return vk::ShaderStageFlagBits::eVertex;
            case engine::Pipeline::Shader::Type::TessellationInit:
                return vk::ShaderStageFlagBits::eTessellationControl;
            case engine::Pipeline::Shader::Type::Tessellation:
                return vk::ShaderStageFlagBits::eTessellationEvaluation;
            case engine::Pipeline::Shader::Type::Geometry:
                return vk::ShaderStageFlagBits::eGeometry;
            case engine::Pipeline::Shader::Type::Pixel:
                return vk::ShaderStageFlagBits::eFragment;
            default:
                throw exception("Invalid shader stage: {}", stage);
        }
    }

    static Shader::TessPrimitive ConvertShaderTessPrimitive(engine::TessellationParameters::DomainType domainType) {
        switch (domainType) {
            case engine::TessellationParameters::DomainType::Isoline:
                return Shader::TessPrimitive::Isolines;
            case engine::TessellationParameters::DomainType::Triangle:
                return Shader::TessPrimitive::Triangles;
            case engine::TessellationParameters::DomainType::Quad:
                return Shader::TessPrimitive::Quads;
        }
    }

    static Shader::TessSpacing ConvertShaderTessSpacing(engine::TessellationParameters::Spacing spacing) {
        switch (spacing) {
            case engine::TessellationParameters::Spacing::Integer:
                return Shader::TessSpacing::Equal;
            case engine::TessellationParameters::Spacing::FractionalEven:
                return Shader::TessSpacing::FractionalEven;
            case engine::TessellationParameters::Spacing::FractionalOdd:
                return Shader::TessSpacing::FractionalOdd;
        }
    }

    static Shader::AttributeType ConvertShaderAttributeType(engine::VertexAttribute attribute) {
        if (attribute.source == engine::VertexAttribute::Source::Inactive)
            return Shader::AttributeType::Disabled;

        switch (attribute.numericalType) {
            case engine::VertexAttribute::NumericalType::Snorm:
            case engine::VertexAttribute::NumericalType::Unorm:
            case engine::VertexAttribute::NumericalType::Sscaled:
            case engine::VertexAttribute::NumericalType::Uscaled:
            case engine::VertexAttribute::NumericalType::Float:
                return Shader::AttributeType::Float;
            case engine::VertexAttribute::NumericalType::Sint:
                return Shader::AttributeType::SignedInt;
            case engine::VertexAttribute::NumericalType::Uint:
                return Shader::AttributeType::UnsignedInt;
            default:
                throw exception("Invalid numerical type: {}", static_cast<u8>(attribute.numericalType));
        }
    }

    /**
     * @notes Roughly based on https://github.com/yuzu-emu/yuzu/blob/4ffbbc534884841f9a5536e57539bf3d1642af26/src/video_core/renderer_vulkan/vk_pipeline_cache.cpp#L127
     */
    static Shader::RuntimeInfo MakeRuntimeInfo(const PackedPipelineState &packedState, Shader::IR::Program &program, Shader::IR::Program *lastProgram, bool hasGeometry) {
        Shader::RuntimeInfo info;
        if (lastProgram) {
            info.previous_stage_stores = lastProgram->info.stores;
            if (lastProgram->is_geometry_passthrough)
                info.previous_stage_stores.mask |= lastProgram->info.passthrough.mask;
        } else {
            info.previous_stage_stores.mask.set();
        }

        switch (program.stage) {
            case Shader::Stage::VertexB:
                if (!hasGeometry) {
                    if (packedState.topology == engine::DrawTopology::Points)
                        info.fixed_state_point_size = packedState.pointSize;

                    //if (key.state.xfb_enabled)
                    //info.xfb_varyings = VideoCommon::MakeTransformFeedbackVaryings(key.state.xfb_state);
                    //}
                    info.convert_depth_mode = packedState.openGlNdc;
                }
                ranges::transform(packedState.vertexAttributes, info.generic_input_types.begin(), &ConvertShaderAttributeType);
                break;
            case Shader::Stage::TessellationEval:
                // Double check this!
                info.tess_clockwise = packedState.outputPrimitives != engine::TessellationParameters::OutputPrimitives::TrianglesCCW;
                info.tess_primitive = ConvertShaderTessPrimitive(packedState.domainType);
                info.tess_spacing = ConvertShaderTessSpacing(packedState.spacing);
                break;
            case Shader::Stage::Geometry:
                if (program.output_topology == Shader::OutputTopology::PointList)
                    info.fixed_state_point_size = packedState.pointSize;

             //   if (key.state.xfb_enabled != 0) {
              //      info.xfb_varyings = VideoCommon::MakeTransformFeedbackVaryings(key.state.xfb_state);
               // }
                info.convert_depth_mode = packedState.openGlNdc;
                break;
            case Shader::Stage::Fragment:
         //       info.alpha_test_func = MaxwellToCompareFunction(
           //         key.state.UnpackComparisonOp(key.state.alpha_test_func.Value()));
             //   info.alpha_test_reference = Common::BitCast<float>(key.state.alpha_test_ref);
                break;
            default:
                break;
        }
        switch (packedState.topology) {
            case engine::DrawTopology::Points:
                info.input_topology = Shader::InputTopology::Points;
                break;
            case engine::DrawTopology::Lines:
            case engine::DrawTopology::LineLoop:
            case engine::DrawTopology::LineStrip:
                info.input_topology = Shader::InputTopology::Lines;
                break;
            case engine::DrawTopology::Triangles:
            case engine::DrawTopology::TriangleStrip:
            case engine::DrawTopology::TriangleFan:
            case engine::DrawTopology::Quads:
            case engine::DrawTopology::QuadStrip:
            case engine::DrawTopology::Polygon:
            case engine::DrawTopology::Patch:
                info.input_topology = Shader::InputTopology::Triangles;
                break;
            case engine::DrawTopology::LineListAdjcy:
            case engine::DrawTopology::LineStripAdjcy:
                info.input_topology = Shader::InputTopology::LinesAdjacency;
                break;
            case engine::DrawTopology::TriangleListAdjcy:
            case engine::DrawTopology::TriangleStripAdjcy:
                info.input_topology = Shader::InputTopology::TrianglesAdjacency;
                break;
        }
        info.force_early_z = packedState.apiMandatedEarlyZ;
        info.y_negate = packedState.flipYEnable;
        return info;
    }

    static std::array<Pipeline::ShaderStage, engine::ShaderStageCount> MakePipelineShaders(InterconnectContext &ctx, const PackedPipelineState &packedState, const std::array<ShaderBinary, engine::PipelineCount> &shaderBinaries) {
        ctx.gpu.shader.ResetPools();

        using PipelineStage = engine::Pipeline::Shader::Type;
        auto pipelineStage{[](size_t i){ return static_cast<PipelineStage>(i); }};
        auto stageIdx{[](PipelineStage stage){ return static_cast<u8>(stage); }};

        std::array<Shader::IR::Program, engine::PipelineCount> programs;
        bool ignoreVertexCullBeforeFetch{};

        for (size_t i{}; i < engine::PipelineCount; i++) {
            if (!packedState.shaderHashes[i])
                continue;

            auto program{ctx.gpu.shader.ParseGraphicsShader(packedState.postVtgShaderAttributeSkipMask,
                                                            ConvertCompilerShaderStage(static_cast<PipelineStage>(i)),
                                                            shaderBinaries[i].binary, shaderBinaries[i].baseOffset,
                                                            packedState.bindlessTextureConstantBufferSlotSelect,
                                                            [](int, int){ return 0; }, [](u32){return Shader::TextureType::Color2D;})};
            if (i == stageIdx(PipelineStage::Vertex) && packedState.shaderHashes[stageIdx(PipelineStage::VertexCullBeforeFetch)]) {
                ignoreVertexCullBeforeFetch = true;
                programs[i] = ctx.gpu.shader.CombineVertexShaders(programs[stageIdx(PipelineStage::VertexCullBeforeFetch)], program, shaderBinaries[i].binary);
            } else {
                programs[i] = program;
            }
        }

        bool hasGeometry{packedState.shaderHashes[stageIdx(PipelineStage::Geometry)] && programs[stageIdx(PipelineStage::Geometry)].is_geometry_passthrough};
        Shader::Backend::Bindings bindings{};
        Shader::IR::Program *lastProgram{};

        std::array<Pipeline::ShaderStage, engine::ShaderStageCount> shaderStages{};

        for (size_t i{stageIdx(ignoreVertexCullBeforeFetch ? PipelineStage::Vertex : PipelineStage::VertexCullBeforeFetch)}; i < engine::ShaderStageCount; i++) {
            if (!packedState.shaderHashes[i])
                continue;

            auto runtimeInfo{MakeRuntimeInfo(packedState, programs[i], lastProgram, hasGeometry)};
            shaderStages[i - (i >= 1 ? 1 : 0)] = {ConvertVkShaderStage(pipelineStage(i)), ctx.gpu.shader.CompileShader(runtimeInfo, programs[i], bindings), programs[i].info};

            lastProgram = &programs[i];
        }

        return shaderStages;
    }

    static Pipeline::DescriptorInfo MakePipelineDescriptorInfo(const std::array<Pipeline::ShaderStage, engine::ShaderStageCount> &shaderStages, bool needsIndividualTextureBindingWrites) {
        Pipeline::DescriptorInfo descriptorInfo{};
        u32 bindingIndex{};

        for (size_t i{}; i < engine::ShaderStageCount; i++) {
            const auto &stage{shaderStages[i]};
            if (!stage.module)
                continue;

            auto &stageCbufUsage{descriptorInfo.cbufUsages[i]};

            auto pushBindings{[&](vk::DescriptorType type, const auto &descs, u32 &count,  auto &&descCb, bool individualDescWrites = false) {
                descriptorInfo.writeDescCount += individualDescWrites ? descs.size() : ((descs.size() > 0) ? 1 : 0);

                for (u32 descIdx{}; descIdx < descs.size(); descIdx++) {
                    const auto &desc{descs[descIdx]};
                    count += desc.count;

                    descCb(desc, descIdx);

                    descriptorInfo.descriptorSetLayoutBindings.push_back(vk::DescriptorSetLayoutBinding{
                        .binding = bindingIndex++,
                        .descriptorType = type,
                        .descriptorCount = desc.count,
                        .stageFlags = stage.stage,
                    });
                }
            }};

            pushBindings(vk::DescriptorType::eUniformBuffer, stage.info.constant_buffer_descriptors, descriptorInfo.uniformBufferDescCount, [&](const Shader::ConstantBufferDescriptor &desc, u32 descIdx) {
                for (u32 cbufIdx{desc.index}; cbufIdx < desc.index + desc.count; cbufIdx++) {
                    auto &usage{stageCbufUsage[cbufIdx]};
                    usage.uniformBuffers.push_back({bindingIndex, descIdx});
                    usage.totalBufferDescCount += desc.count;
                    usage.writeDescCount++;
                }
            });
            pushBindings(vk::DescriptorType::eStorageBuffer, stage.info.storage_buffers_descriptors, descriptorInfo.storageBufferDescCount, [&](const Shader::StorageBufferDescriptor &desc, u32 descIdx) {
                auto &usage{stageCbufUsage[desc.cbuf_index]};
                usage.storageBuffers.push_back({bindingIndex, descIdx});
                usage.totalBufferDescCount += desc.count;
                usage.writeDescCount++;
            });
            descriptorInfo.totalBufferDescCount += descriptorInfo.uniformBufferDescCount + descriptorInfo.storageBufferDescCount;

            pushBindings(vk::DescriptorType::eUniformTexelBuffer, stage.info.texture_buffer_descriptors, descriptorInfo.uniformTexelBufferDescCount, [](const auto &, u32) {});
            pushBindings(vk::DescriptorType::eStorageTexelBuffer, stage.info.image_buffer_descriptors, descriptorInfo.storageTexelBufferDescCount, [](const auto &, u32) {});
            descriptorInfo.totalTexelBufferDescCount += descriptorInfo.uniformTexelBufferDescCount + descriptorInfo.storageTexelBufferDescCount;

            pushBindings(vk::DescriptorType::eCombinedImageSampler, stage.info.texture_descriptors, descriptorInfo.combinedImageSamplerDescCount, [](const auto &, u32) {}, needsIndividualTextureBindingWrites);
            pushBindings(vk::DescriptorType::eStorageImage, stage.info.image_descriptors, descriptorInfo.storageImageDescCount, [](const auto &, u32) {});
            descriptorInfo.totalImageDescCount += descriptorInfo.combinedImageSamplerDescCount + descriptorInfo.storageImageDescCount;
        }

        descriptorInfo.totalElemCount = descriptorInfo.totalBufferDescCount + descriptorInfo.totalTexelBufferDescCount + descriptorInfo.totalImageDescCount;
        return descriptorInfo;
    }

    static vk::Format ConvertVertexInputAttributeFormat(engine::VertexAttribute::ComponentBitWidths componentBitWidths, engine::VertexAttribute::NumericalType numericalType) {
        #define FORMAT_CASE(bitWidths, type, vkType, vkFormat, ...) \
            case engine::VertexAttribute::ComponentBitWidths::bitWidths | engine::VertexAttribute::NumericalType::type: \
                return vk::Format::vkFormat ## vkType ##__VA_ARGS__

        #define FORMAT_INT_CASE(size, vkFormat, ...) \
            FORMAT_CASE(size, Uint, Uint, vkFormat, ##__VA_ARGS__); \
            FORMAT_CASE(size, Sint, Sint, vkFormat, ##__VA_ARGS__);

        #define FORMAT_INT_FLOAT_CASE(size, vkFormat, ...) \
            FORMAT_INT_CASE(size, vkFormat, ##__VA_ARGS__); \
            FORMAT_CASE(size, Float, Sfloat, vkFormat, ##__VA_ARGS__);

        #define FORMAT_NORM_INT_SCALED_CASE(size, vkFormat, ...) \
            FORMAT_INT_CASE(size, vkFormat, ##__VA_ARGS__);               \
            FORMAT_CASE(size, Unorm, Unorm, vkFormat, ##__VA_ARGS__);     \
            FORMAT_CASE(size, Snorm, Unorm, vkFormat, ##__VA_ARGS__);     \
            FORMAT_CASE(size, Uscaled, Uscaled, vkFormat, ##__VA_ARGS__); \
            FORMAT_CASE(size, Sscaled, Sscaled, vkFormat, ##__VA_ARGS__)

        #define FORMAT_NORM_INT_SCALED_FLOAT_CASE(size, vkFormat) \
            FORMAT_NORM_INT_SCALED_CASE(size, vkFormat); \
            FORMAT_CASE(size, Float, Sfloat, vkFormat)

        switch (componentBitWidths | numericalType) {
                /* 8-bit components */
            FORMAT_NORM_INT_SCALED_CASE(R8, eR8);
            FORMAT_NORM_INT_SCALED_CASE(R8_G8, eR8G8);
            FORMAT_NORM_INT_SCALED_CASE(G8R8, eR8G8);
            FORMAT_NORM_INT_SCALED_CASE(R8_G8_B8, eR8G8B8);
            FORMAT_NORM_INT_SCALED_CASE(R8_G8_B8_A8, eR8G8B8A8);
            FORMAT_NORM_INT_SCALED_CASE(A8B8G8R8, eR8G8B8A8);
            FORMAT_NORM_INT_SCALED_CASE(X8B8G8R8, eR8G8B8A8);

                /* 16-bit components */
            FORMAT_NORM_INT_SCALED_FLOAT_CASE(R16, eR16);
            FORMAT_NORM_INT_SCALED_FLOAT_CASE(R16_G16, eR16G16);
            FORMAT_NORM_INT_SCALED_FLOAT_CASE(R16_G16_B16, eR16G16B16);
            FORMAT_NORM_INT_SCALED_FLOAT_CASE(R16_G16_B16_A16, eR16G16B16A16);

                /* 32-bit components */
            FORMAT_INT_FLOAT_CASE(R32, eR32);
            FORMAT_INT_FLOAT_CASE(R32_G32, eR32G32);
            FORMAT_INT_FLOAT_CASE(R32_G32_B32, eR32G32B32);
            FORMAT_INT_FLOAT_CASE(R32_G32_B32_A32, eR32G32B32A32);

                /* 10-bit RGB, 2-bit A */
            FORMAT_NORM_INT_SCALED_CASE(A2B10G10R10, eA2B10G10R10, Pack32);

                /* 11-bit G and R, 10-bit B */
            FORMAT_CASE(B10G11R11, Float, Ufloat, eB10G11R11, Pack32);

            default:
                Logger::Warn("Unimplemented Maxwell3D Vertex Buffer Format: {} | {}", static_cast<u8>(componentBitWidths), static_cast<u8>(numericalType));
                return vk::Format::eR8G8B8A8Unorm;
        }

        #undef FORMAT_CASE
        #undef FORMAT_INT_CASE
        #undef FORMAT_INT_FLOAT_CASE
        #undef FORMAT_NORM_INT_SCALED_CASE
        #undef FORMAT_NORM_INT_SCALED_FLOAT_CASE
    }

    static vk::PrimitiveTopology ConvertPrimitiveTopology(engine::DrawTopology topology) {
        switch (topology) {
            case engine::DrawTopology::Points:
                return vk::PrimitiveTopology::ePointList;
            case engine::DrawTopology::Lines:
                return vk::PrimitiveTopology::eLineList;
            case engine::DrawTopology::LineStrip:
                return vk::PrimitiveTopology::eLineStrip;
            case engine::DrawTopology::Triangles:
                return vk::PrimitiveTopology::eTriangleList;
            case engine::DrawTopology::TriangleStrip:
                return vk::PrimitiveTopology::eTriangleStrip;
            case engine::DrawTopology::TriangleFan:
                return vk::PrimitiveTopology::eTriangleFan;
            case engine::DrawTopology::Quads:
                return vk::PrimitiveTopology::eTriangleList; // Uses quad conversion
            case engine::DrawTopology::LineListAdjcy:
                return vk::PrimitiveTopology::eLineListWithAdjacency;
            case engine::DrawTopology::LineStripAdjcy:
                return vk::PrimitiveTopology::eLineStripWithAdjacency;
            case engine::DrawTopology::TriangleListAdjcy:
                return vk::PrimitiveTopology::eTriangleListWithAdjacency;
            case engine::DrawTopology::TriangleStripAdjcy:
                return vk::PrimitiveTopology::eTriangleStripWithAdjacency;
            case engine::DrawTopology::Patch:
                return vk::PrimitiveTopology::ePatchList;
            default:
                Logger::Warn("Unimplemented input assembly topology: {}", static_cast<u8>(topology));
                return vk::PrimitiveTopology::eTriangleList;
        }
    }

    static vk::ProvokingVertexModeEXT ConvertProvokingVertex(engine::ProvokingVertex::Value provokingVertex) {
        switch (provokingVertex) {
            case engine::ProvokingVertex::Value::First:
                return vk::ProvokingVertexModeEXT::eFirstVertex;
            case engine::ProvokingVertex::Value::Last:
                return vk::ProvokingVertexModeEXT::eLastVertex;
        }
    }

    static cache::GraphicsPipelineCache::CompiledPipeline MakeCompiledPipeline(InterconnectContext &ctx,
                                                                               const PackedPipelineState &packedState,
                                                                               const std::array<Pipeline::ShaderStage, engine::ShaderStageCount> &shaderStages,
                                                                               span<vk::DescriptorSetLayoutBinding> layoutBindings,
                                                                               span<TextureView *> colorAttachments, TextureView *depthAttachment) {
        boost::container::static_vector<vk::PipelineShaderStageCreateInfo, engine::ShaderStageCount> shaderStageInfos;
        for (const auto &stage : shaderStages)
            if (stage.module)
                shaderStageInfos.push_back(vk::PipelineShaderStageCreateInfo{
                    .stage = stage.stage,
                    .module = &*stage.module,
                    .pName = "main"
                });

        boost::container::static_vector<vk::VertexInputBindingDescription, engine::VertexStreamCount> bindingDescs;
        boost::container::static_vector<vk::VertexInputBindingDivisorDescriptionEXT, engine::VertexStreamCount> bindingDivisorDescs;
        boost::container::static_vector<vk::VertexInputAttributeDescription, engine::VertexAttributeCount> attributeDescs;

        for (u32 i{}; i < engine::VertexStreamCount; i++) {
            const auto &binding{packedState.vertexBindings[i]};
            bindingDescs.push_back({
                .binding = i,
                .stride = binding.stride,
                .inputRate = binding.divisor ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex,
            });

            if (binding.GetInputRate() == vk::VertexInputRate::eInstance) {
                if (!ctx.gpu.traits.supportsVertexAttributeDivisor) [[unlikely]]
                        Logger::Warn("Vertex attribute divisor used on guest without host support");
                else if (!ctx.gpu.traits.supportsVertexAttributeZeroDivisor && binding.divisor == 0) [[unlikely]]
                        Logger::Warn("Vertex attribute zero divisor used on guest without host support");
                else
                    bindingDivisorDescs.push_back({
                        .binding = i,
                        .divisor = binding.divisor,
                    });
            }
        }

        for (u32 i{}; i < engine::VertexAttributeCount; i++) {
            const auto &attribute{packedState.vertexAttributes[i]};
            if (attribute.source == engine::VertexAttribute::Source::Active && shaderStages[0].info.loads.Generic(i))
                attributeDescs.push_back({
                    .location = i,
                    .binding = attribute.stream,
                    .format = ConvertVertexInputAttributeFormat(attribute.componentBitWidths, attribute.numericalType),
                    .offset = attribute.offset,
                });
        }

        vk::StructureChain<vk::PipelineVertexInputStateCreateInfo, vk::PipelineVertexInputDivisorStateCreateInfoEXT> vertexInputState{
            vk::PipelineVertexInputStateCreateInfo{
                .vertexBindingDescriptionCount = static_cast<u32>(bindingDescs.size()),
                .pVertexBindingDescriptions = bindingDescs.data(),
                .vertexAttributeDescriptionCount = static_cast<u32>(attributeDescs.size()),
                .pVertexAttributeDescriptions = attributeDescs.data(),
            },
            vk::PipelineVertexInputDivisorStateCreateInfoEXT{
                .vertexBindingDivisorCount = static_cast<u32>(bindingDivisorDescs.size()),
                .pVertexBindingDivisors = bindingDivisorDescs.data(),
            },
        };

        if (bindingDivisorDescs.empty())
            vertexInputState.unlink<vk::PipelineVertexInputDivisorStateCreateInfoEXT>();

        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState{
            .topology = ConvertPrimitiveTopology(packedState.topology),
            .primitiveRestartEnable = packedState.primitiveRestartEnabled,
        };

        vk::PipelineTessellationStateCreateInfo tessellationState{
            .patchControlPoints = packedState.patchSize,
        };

        vk::StructureChain<vk::PipelineRasterizationStateCreateInfo, vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT> rasterizationState{};

        auto &rasterizationCreateInfo{rasterizationState.get<vk::PipelineRasterizationStateCreateInfo>()};
        rasterizationCreateInfo.rasterizerDiscardEnable = packedState.rasterizerDiscardEnable;
        rasterizationCreateInfo.polygonMode = packedState.GetPolygonMode();
        rasterizationCreateInfo.cullMode = vk::CullModeFlags{packedState.cullMode};
        rasterizationCreateInfo.frontFace = packedState.frontFaceClockwise ? vk::FrontFace::eClockwise : vk::FrontFace::eCounterClockwise;
        rasterizationCreateInfo.depthBiasEnable = packedState.depthBiasEnable;
        rasterizationState.get<vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT>().provokingVertexMode = ConvertProvokingVertex(packedState.provokingVertex);

        constexpr vk::PipelineMultisampleStateCreateInfo multisampleState{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
        };

        vk::PipelineDepthStencilStateCreateInfo depthStencilState{
            .depthTestEnable = packedState.depthTestEnable,
            .depthWriteEnable = packedState.depthWriteEnable,
            .depthCompareOp = packedState.GetDepthFunc(),
            .depthBoundsTestEnable = packedState.depthBoundsTestEnable,
            .stencilTestEnable = packedState.stencilTestEnable
        };

        std::tie(depthStencilState.front, depthStencilState.back) = packedState.GetStencilOpsState();

        boost::container::static_vector<vk::PipelineColorBlendAttachmentState, engine::ColorTargetCount> attachmentBlendStates;
        for (u32 i{}; i < colorAttachments.size(); i++)
            attachmentBlendStates.push_back(packedState.GetAttachmentBlendState(i));

        vk::PipelineColorBlendStateCreateInfo colorBlendState{
            .logicOpEnable = packedState.logicOpEnable,
            .logicOp = packedState.GetLogicOp(),
            .attachmentCount = static_cast<u32>(attachmentBlendStates.size()),
            .pAttachments = attachmentBlendStates.data()
        };

        constexpr std::array<vk::DynamicState, 9> dynamicStates{
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
            vk::DynamicState::eLineWidth,
            vk::DynamicState::eDepthBias,
            vk::DynamicState::eBlendConstants,
            vk::DynamicState::eDepthBounds,
            vk::DynamicState::eStencilCompareMask,
            vk::DynamicState::eStencilWriteMask,
            vk::DynamicState::eStencilReference
        };

        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<u32>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };

        // Dynamic state will be used instead of these
        std::array<vk::Rect2D, engine::ViewportCount> emptyScissors{};
        std::array<vk::Viewport, engine::ViewportCount> emptyViewports{};

        vk::PipelineViewportStateCreateInfo viewportState{
            .viewportCount = static_cast<u32>(ctx.gpu.traits.supportsMultipleViewports ? engine::ViewportCount : 1),
            .pViewports = emptyViewports.data(),
            .scissorCount = static_cast<u32>(ctx.gpu.traits.supportsMultipleViewports ? engine::ViewportCount : 1),
            .pScissors = emptyScissors.data(),
        };

        return ctx.gpu.graphicsPipelineCache.GetCompiledPipeline(cache::GraphicsPipelineCache::PipelineState{
            .shaderStages = shaderStageInfos,
            .vertexState = vertexInputState,
            .inputAssemblyState = inputAssemblyState,
            .tessellationState = tessellationState,
            .viewportState = viewportState,
            .rasterizationState = rasterizationState,
            .multisampleState = multisampleState,
            .depthStencilState = depthStencilState,
            .colorBlendState = colorBlendState,
            .dynamicState = dynamicState,
            .colorAttachments = colorAttachments,
            .depthStencilAttachment = depthAttachment,
        }, layoutBindings);
    }

    Pipeline::Pipeline(InterconnectContext &ctx, const PackedPipelineState &packedState, const std::array<ShaderBinary, engine::PipelineCount> &shaderBinaries, span<TextureView *> colorAttachments, TextureView *depthAttachment)
        : shaderStages{MakePipelineShaders(ctx, packedState, shaderBinaries)},
          descriptorInfo{MakePipelineDescriptorInfo(shaderStages, ctx.gpu.traits.quirks.needsIndividualTextureBindingWrites)},
          compiledPipeline{MakeCompiledPipeline(ctx, packedState, shaderStages, descriptorInfo.descriptorSetLayoutBindings, colorAttachments, depthAttachment)},
          sourcePackedState{packedState} {
        storageBufferViews.resize(descriptorInfo.storageBufferDescCount);
    }

    Pipeline *Pipeline::LookupNext(const PackedPipelineState &packedState) {
        auto it{std::find_if(transitionCache.begin(), transitionCache.end(), [&packedState](auto pipeline) {
            if (pipeline && pipeline->sourcePackedState == packedState)
                return true;
            else
                return false;
        })};

        if (it != transitionCache.end())
            return *it;

        return nullptr;
    }

    void Pipeline::AddTransition(Pipeline *next) {
        transitionCache[transitionCacheNextIdx] = next;
        transitionCacheNextIdx = (transitionCacheNextIdx + 1) % transitionCache.size();
    }

    bool Pipeline::CheckBindingMatch(Pipeline *other) {
        if (auto it{bindingMatchCache.find(other)}; it != bindingMatchCache.end())
            return it->second;

        for (size_t i{}; i < shaderStages.size(); i++) {
            if (!shaderStages[i].BindingsEqual(other->shaderStages[i])) {
                bindingMatchCache[other] = false;
                return false;
            }
        }

        bindingMatchCache[other] = true;
        return true;
    }

    static DynamicBufferBinding GetConstantBufferBinding(InterconnectContext &ctx, const Shader::Info &info, BufferView view, size_t idx) {
        ctx.executor.AttachBuffer(view);

        size_t sizeOverride{std::min<size_t>(info.constant_buffer_used_sizes[idx], view.size)};
        if (auto megaBufferBinding{view.TryMegaBuffer(ctx.executor.cycle, ctx.executor.AcquireMegaBufferAllocator(), ctx.executor.executionNumber, sizeOverride)}) {
            return megaBufferBinding;
        } else {
            view.GetBuffer()->BlockSequencedCpuBackingWrites();
            return view;
        }
    }

    static DynamicBufferBinding GetStorageBufferBinding(InterconnectContext &ctx, const Shader::Info &info, ConstantBuffer &cbuf, CachedMappedBufferView &cachedView, size_t idx) {
        struct SsboDescriptor {
            u64 address;
            u32 size;
        };

        const auto &desc{info.storage_buffers_descriptors[idx]};
        auto ssbo{cbuf.Read<SsboDescriptor>(ctx.executor, desc.cbuf_offset)};
        cachedView.Update(ctx, ssbo.address, ssbo.size);

        auto view{cachedView.view};
        ctx.executor.AttachBuffer(view);
        view.GetBuffer()->BlockSequencedCpuBackingWrites();

        if (desc.is_written)
            view.GetBuffer()->MarkGpuDirty();

        return view;
    }

    // TODO: EXEC ID FOR STORAGE BUFS PURGE REMAP
    DescriptorUpdateInfo *Pipeline::SyncDescriptors(InterconnectContext &ctx, ConstantBufferSet &constantBuffers) {
        u32 bindingIdx{};
        u32 writeIdx{};
        u32 bufferIdx{};
        u32 imageIdx{};

        auto writes{ctx.executor.allocator->AllocateUntracked<vk::WriteDescriptorSet>(descriptorInfo.writeDescCount)};
        auto bufferDescs{ctx.executor.allocator->AllocateUntracked<vk::DescriptorBufferInfo>(descriptorInfo.totalBufferDescCount)};
        auto bufferDescDynamicBindings{ctx.executor.allocator->AllocateUntracked<DynamicBufferBinding>(descriptorInfo.totalBufferDescCount)};

        auto writeBufferDescs{[&](vk::DescriptorType type, const auto &descs, u32 count, auto getBufferCb) {
            if (!descs.empty()) {
                writes[writeIdx++] = {
                    .dstBinding = bindingIdx,
                    .descriptorCount = count,
                    .descriptorType = type,
                    .pBufferInfo = &bufferDescs[bufferIdx],
                };

                for (size_t descIdx{}; descIdx < descs.size(); descIdx++) {
                    const auto &shaderDesc{descs[descIdx]};
                    for (size_t arrayIdx{}; arrayIdx < shaderDesc.count; arrayIdx++)
                        bufferDescDynamicBindings[bufferIdx++] = getBufferCb(shaderDesc, descIdx, arrayIdx);
                }
            }
        }};

        for (size_t i{}; i < shaderStages.size(); i++) {
            const auto &stage{shaderStages[i]};
            if (!stage.module)
                continue;

            writeBufferDescs(vk::DescriptorType::eUniformBuffer, stage.info.constant_buffer_descriptors, descriptorInfo.uniformBufferDescCount,
                             [&](const Shader::ConstantBufferDescriptor &desc, size_t descIdx, size_t arrayIdx) {
                size_t cbufIdx{desc.index + arrayIdx};
                return GetConstantBufferBinding(ctx, stage.info, constantBuffers[i][cbufIdx].view, cbufIdx);
            });

            writeBufferDescs(vk::DescriptorType::eStorageBuffer, stage.info.storage_buffers_descriptors, descriptorInfo.storageBufferDescCount,
                             [&](const Shader::StorageBufferDescriptor &desc, size_t descIdx, size_t arrayIdx) {
                return GetStorageBufferBinding(ctx, stage.info, constantBuffers[i][desc.cbuf_index], storageBufferViews[descIdx], descIdx);
            });
        }
        
        return ctx.executor.allocator->EmplaceUntracked<DescriptorUpdateInfo>(DescriptorUpdateInfo{
            .writes = writes,
            .bufferDescs = bufferDescs,
            .bufferDescDynamicBindings = bufferDescDynamicBindings,
            .pipelineLayout = compiledPipeline.pipelineLayout,
            .descriptorSetLayout = compiledPipeline.descriptorSetLayout,
            .bindPoint = vk::PipelineBindPoint::eGraphics,
            .descriptorSetIndex = 0,
        });
    }

    DescriptorUpdateInfo *Pipeline::SyncDescriptorsQuickBind(InterconnectContext &ctx, ConstantBufferSet &constantBuffers, ConstantBuffers::QuickBind quickBind) {
        size_t stageIndex{static_cast<size_t>(quickBind.stage)};

        const auto &cbufUsageInfo{descriptorInfo.cbufUsages[stageIndex][quickBind.index]};
        const auto &shaderInfo{shaderStages[stageIndex].info};
        auto &stageConstantBuffers{constantBuffers[stageIndex]};
        auto copies{ctx.executor.allocator->AllocateUntracked<vk::CopyDescriptorSet>(1)};
        auto writes{ctx.executor.allocator->AllocateUntracked<vk::WriteDescriptorSet>(cbufUsageInfo.writeDescCount)};

        size_t writeIdx{};
        size_t bufferIdx{};

        auto bufferDescs{ctx.executor.allocator->AllocateUntracked<vk::DescriptorBufferInfo>(cbufUsageInfo.totalBufferDescCount)};
        auto bufferDescDynamicBindings{ctx.executor.allocator->AllocateUntracked<DynamicBufferBinding>(cbufUsageInfo.totalBufferDescCount)};

        // TODO: opt this to do partial copy and avoid updating twice
        copies[0] = vk::CopyDescriptorSet{
            .srcBinding = 0,
            .srcArrayElement = 0,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = descriptorInfo.totalElemCount,
        };

        auto writeBufferDescs{[&](vk::DescriptorType type, const auto &usages, const auto &descs, u32 count, auto getBufferCb) {
            for (const auto &usage : usages) {
                const auto &shaderDesc{descs[usage.shaderDescIdx]};

                writes[writeIdx++] = {
                    .dstBinding = usage.binding,
                    .descriptorCount = shaderDesc.count,
                    .descriptorType = type,
                    .pBufferInfo = &bufferDescs[bufferIdx],
                };

                for (size_t i{}; i < shaderDesc.count; i++)
                    bufferDescDynamicBindings[bufferIdx++] = getBufferCb(shaderDesc, usage.shaderDescIdx, i);
            }
        }};

        writeBufferDescs(vk::DescriptorType::eUniformBuffer, cbufUsageInfo.uniformBuffers, shaderInfo.constant_buffer_descriptors, descriptorInfo.uniformBufferDescCount,
                         [&](const Shader::ConstantBufferDescriptor &desc, size_t descIdx, size_t arrayIdx) -> DynamicBufferBinding {
            size_t cbufIdx{desc.index + arrayIdx};
            return GetConstantBufferBinding(ctx, shaderInfo, stageConstantBuffers[cbufIdx].view, cbufIdx);
        });

        writeBufferDescs(vk::DescriptorType::eStorageBuffer, cbufUsageInfo.storageBuffers, shaderInfo.storage_buffers_descriptors, descriptorInfo.storageBufferDescCount,
                         [&](const Shader::StorageBufferDescriptor &desc, size_t descIdx, size_t arrayIdx) {
            return GetStorageBufferBinding(ctx, shaderInfo, stageConstantBuffers[desc.cbuf_index], storageBufferViews[bufferIdx], descIdx);
        });

        return ctx.executor.allocator->EmplaceUntracked<DescriptorUpdateInfo>(DescriptorUpdateInfo{
            .copies = copies,
            .writes = writes,
            .bufferDescs = bufferDescs,
            .bufferDescDynamicBindings = bufferDescDynamicBindings,
            .pipelineLayout = compiledPipeline.pipelineLayout,
            .descriptorSetLayout = compiledPipeline.descriptorSetLayout,
            .bindPoint = vk::PipelineBindPoint::eGraphics,
            .descriptorSetIndex = 0,
        });
    }
}

