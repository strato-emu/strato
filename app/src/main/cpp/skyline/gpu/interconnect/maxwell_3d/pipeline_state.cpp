// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/Ryujinx/)
// Copyright © 2022 yuzu Team and Contributors (https://github.com/yuzu-emu/)
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <optional>
#include <range/v3/algorithm/for_each.hpp>
#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include <gpu/interconnect/command_executor.h>
#include <gpu/texture/format.h>
#include <gpu.h>
#include "pipeline_state.h"

namespace skyline::gpu::interconnect::maxwell3d {
    /* Colour Render Target */
    void ColorRenderTargetState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, colorTarget);
    }

    ColorRenderTargetState::ColorRenderTargetState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    static texture::Format ConvertColorRenderTargetFormat(engine::ColorTarget::Format format) {
        #define FORMAT_CASE_BASE(engineFormat, skFormat, warn) \
                case engine::ColorTarget::Format::engineFormat:                     \
                    if constexpr (warn)                                             \
                        Logger::Warn("Partially supported RT format: " #engineFormat " used!"); \
                    return skyline::gpu::format::skFormat

        #define FORMAT_CASE(engineFormat, skFormat) FORMAT_CASE_BASE(engineFormat, skFormat, false)
        #define FORMAT_CASE_WARN(engineFormat, skFormat) FORMAT_CASE_BASE(engineFormat, skFormat, true)

        switch (format) {
            FORMAT_CASE(RF32_GF32_BF32_AF32, R32G32B32A32Float);
            FORMAT_CASE(RS32_GS32_BS32_AS32, R32G32B32A32Sint);
            FORMAT_CASE(RU32_GU32_BU32_AU32, R32G32B32A32Uint);
            FORMAT_CASE_WARN(RF32_GF32_BF32_X32, R32G32B32A32Float); // TODO: ignore X32 component with blend
            FORMAT_CASE_WARN(RS32_GS32_BS32_X32, R32G32B32A32Sint); // TODO: ^
            FORMAT_CASE_WARN(RU32_GU32_BU32_X32, R32G32B32A32Uint); // TODO: ^
            FORMAT_CASE(R16_G16_B16_A16, R16G16B16A16Unorm);
            FORMAT_CASE(RN16_GN16_BN16_AN16, R16G16B16A16Snorm);
            FORMAT_CASE(RS16_GS16_BS16_AS16, R16G16B16A16Sint);
            FORMAT_CASE(RU16_GU16_BU16_AU16, R16G16B16A16Uint);
            FORMAT_CASE(RF16_GF16_BF16_AF16, R16G16B16A16Float);
            FORMAT_CASE(RF32_GF32, R32G32Float);
            FORMAT_CASE(RS32_GS32, R32G32Sint);
            FORMAT_CASE(RU32_GU32, R32G32Uint);
            FORMAT_CASE_WARN(RF16_GF16_BF16_X16, R16G16B16A16Float); // TODO: ^^
            FORMAT_CASE(A8R8G8B8, B8G8R8A8Unorm);
            FORMAT_CASE(A8RL8GL8BL8, B8G8R8A8Srgb);
            FORMAT_CASE(A2B10G10R10, A2B10G10R10Unorm);
            FORMAT_CASE(AU2BU10GU10RU10, A2B10G10R10Uint);
            FORMAT_CASE(A8B8G8R8, R8G8B8A8Unorm);
            FORMAT_CASE(A8BL8GL8RL8, R8G8B8A8Srgb);
            FORMAT_CASE(AN8BN8GN8RN8, R8G8B8A8Snorm);
            FORMAT_CASE(AS8BS8GS8RS8, R8G8B8A8Sint);
            FORMAT_CASE(R16_G16, R16G16Unorm);
            FORMAT_CASE(RN16_GN16, R16G16Snorm);
            FORMAT_CASE(RS16_GS16, R16G16Sint);
            FORMAT_CASE(RU16_GU16, R16G16Uint);
            FORMAT_CASE(RF16_GF16, R16G16Float);
            FORMAT_CASE(A2R10G10B10, A2B10G10R10Unorm);
            FORMAT_CASE(BF10GF11RF11, B10G11R11Float);
            FORMAT_CASE(RS32, R32Sint);
            FORMAT_CASE(RU32, R32Uint);
            FORMAT_CASE(RF32, R32Float);
            FORMAT_CASE_WARN(X8R8G8B8, B8G8R8A8Unorm); // TODO: ^^
            FORMAT_CASE_WARN(X8RL8GL8BL8, B8G8R8A8Srgb); // TODO: ^^
            FORMAT_CASE(R5G6B5, R5G6B5Unorm);
            FORMAT_CASE(A1R5G5B5, A1R5G5B5Unorm);
            FORMAT_CASE(G8R8, R8G8Unorm);
            FORMAT_CASE(GN8RN8, R8G8Snorm);
            FORMAT_CASE(GS8RS8, R8G8Sint);
            FORMAT_CASE(GU8RU8, R8G8Uint);
            FORMAT_CASE(R16, R16Unorm);
            FORMAT_CASE(RN16, R16Snorm);
            FORMAT_CASE(RS16, R16Sint);
            FORMAT_CASE(RU16, R16Uint);
            FORMAT_CASE(RF16, R16Float);
            FORMAT_CASE(R8, R8Unorm);
            FORMAT_CASE(RN8, R8Snorm);
            FORMAT_CASE(RS8, R8Sint);
            FORMAT_CASE(RU8, R8Uint);
            // FORMAT_CASE(A8, A8Unorm);
            FORMAT_CASE_WARN(X1R5G5B5, A1R5G5B5Unorm); // TODO: ^^
            FORMAT_CASE_WARN(X8B8G8R8, R8G8B8A8Unorm); // TODO: ^^
            FORMAT_CASE_WARN(X8BL8GL8RL8, R8G8B8A8Srgb); // TODO: ^^
            FORMAT_CASE_WARN(Z1R5G5B5, A1R5G5B5Unorm); // TODO: ^^ but with zero blend
            FORMAT_CASE_WARN(O1R5G5B5, A1R5G5B5Unorm); // TODO: ^^ but with one blend
            FORMAT_CASE_WARN(Z8R8G8B8, B8G8R8A8Unorm); // TODO: ^^ but with zero blend
            FORMAT_CASE_WARN(O8R8G8B8, B8G8R8A8Unorm); // TODO: ^^ but with one blend
            // FORMAT_CASE(R32, R32Unorm);
            // FORMAT_CASE(A16, A16Unorm);
            // FORMAT_CASE(AF16, A16Float);
            // FORMAT_CASE(AF32, A32Float);
            // FORMAT_CASE(A8R8, R8A8Unorm);
            // FORMAT_CASE(R16_A16, R16A16Unorm);
            // FORMAT_CASE(RF16_AF16, R16A16Float);
            // FORMAT_CASE(RF32_AF32, R32A32Float);
            // FORMAT_CASE(B8G8R8A8, A8R8G8B8Unorm)
            default:
                throw exception("Unsupported colour rendertarget format: 0x{:X}", static_cast<u32>(format));
        }

        #undef FORMAT_CASE
        #undef FORMAT_CASE_WARN
        #undef FORMAT_CASE_BASE
    }

    void ColorRenderTargetState::Flush(InterconnectContext &ctx) {
        auto &target{engine->colorTarget};
        if (target.format == engine::ColorTarget::Format::Disabled) {
            view = {};
            return;
        }

        GuestTexture guest{};
        guest.format = ConvertColorRenderTargetFormat(target.format);
        guest.aspect = vk::ImageAspectFlagBits::eColor;
        guest.baseArrayLayer = target.layerOffset;

        bool thirdDimensionDefinesArraySize{target.memory.thirdDimensionControl == engine::TargetMemory::ThirdDimensionControl::ThirdDimensionDefinesArraySize};
        guest.layerCount = thirdDimensionDefinesArraySize ? target.thirdDimension : 1;
        guest.viewType = target.thirdDimension > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D;

        u32 depth{thirdDimensionDefinesArraySize ? 1U : target.thirdDimension};
        if (target.memory.layout == engine::TargetMemory::Layout::Pitch) {
            guest.dimensions = texture::Dimensions{target.width / guest.format->bpb, target.height, depth};
            guest.tileConfig = texture::TileConfig{
                .mode = gpu::texture::TileMode::Linear,
            };
        } else {
            guest.dimensions = gpu::texture::Dimensions{target.width, target.height, depth};
            guest.tileConfig = gpu::texture::TileConfig{
                .mode = gpu::texture::TileMode::Block,
                .blockHeight = target.memory.BlockHeight(),
                .blockDepth = target.memory.BlockDepth(),
            };
        }

        guest.layerStride = (guest.baseArrayLayer > 1 || guest.layerCount > 1) ? target.ArrayPitch() : 0;

        auto mappings{ctx.channelCtx.asCtx->gmmu.TranslateRange(target.offset, guest.GetSize())};
        guest.mappings.assign(mappings.begin(), mappings.end());

        view = ctx.executor.AcquireTextureManager().FindOrCreate(guest, ctx.executor.tag);
    }

    /* Depth Render Target */
    void DepthRenderTargetState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, ztSize, ztOffset, ztFormat, ztBlockSize, ztArrayPitch, ztSelect, ztLayer);
    }

    DepthRenderTargetState::DepthRenderTargetState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    static texture::Format ConvertDepthRenderTargetFormat(engine::ZtFormat format) {
        #define FORMAT_CASE(engineFormat, skFormat) \
            case engine::ZtFormat::engineFormat: \
                return skyline::gpu::format::skFormat

        switch (format) {
            FORMAT_CASE(Z16, D16Unorm);
            FORMAT_CASE(Z24S8, S8UintD24Unorm);
            FORMAT_CASE(X8Z24, D24UnormX8Uint);
            FORMAT_CASE(S8Z24, D24UnormS8Uint);
            FORMAT_CASE(S8, S8Uint);
            FORMAT_CASE(ZF32, D32Float);
            FORMAT_CASE(ZF32_X24S8, D32FloatS8Uint);
            default:
                throw exception("Unsupported depth rendertarget format: 0x{:X}", static_cast<u32>(format));
        }

        #undef FORMAT_CASE
    }

    void DepthRenderTargetState::Flush(InterconnectContext &ctx) {
        if (!engine->ztSelect.targetCount) {
            view = {};
            return;
        }

        GuestTexture guest{};
        guest.format = ConvertDepthRenderTargetFormat(engine->ztFormat);
        guest.aspect = guest.format->vkAspect;
        guest.baseArrayLayer = engine->ztLayer.offset;

        bool thirdDimensionDefinesArraySize{engine->ztSize.control == engine::ZtSize::Control::ThirdDimensionDefinesArraySize};
        if (engine->ztSize.control == engine::ZtSize::Control::ThirdDimensionDefinesArraySize) {
            guest.layerCount = engine->ztSize.thirdDimension;
            guest.viewType = vk::ImageViewType::e2DArray;
        } else if (engine->ztSize.control == engine::ZtSize::Control::ArraySizeIsOne) {
            guest.layerCount = 1;
            guest.viewType = vk::ImageViewType::e2D;
        }

        guest.dimensions = gpu::texture::Dimensions{engine->ztSize.width, engine->ztSize.height, 1};
        guest.tileConfig = gpu::texture::TileConfig{
            .mode = gpu::texture::TileMode::Block,
            .blockHeight = engine->ztBlockSize.BlockHeight(),
            .blockDepth = engine->ztBlockSize.BlockDepth(),
        };

        guest.layerStride = (guest.baseArrayLayer > 1 || guest.layerCount > 1) ? engine->ztArrayPitch : 0;

        auto mappings{ctx.channelCtx.asCtx->gmmu.TranslateRange(engine->ztOffset, guest.GetSize())};
        guest.mappings.assign(mappings.begin(), mappings.end());

        view = ctx.executor.AcquireTextureManager().FindOrCreate(guest, ctx.executor.tag);
    }

    /* Vertex Input State */
    void VertexInputState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        ranges::for_each(vertexStreamRegisters, [&](const auto &regs) { manager.Bind(handle, regs.format, regs.frequency); });

        auto bindFull{[&](const auto &regs) { manager.Bind(handle, regs); }};
        ranges::for_each(vertexStreamInstanceRegisters, bindFull);
        ranges::for_each(vertexAttributesRegisters, bindFull);
    }

    vk::StructureChain<vk::PipelineVertexInputStateCreateInfo, vk::PipelineVertexInputDivisorStateCreateInfoEXT> VertexInputState::Build(InterconnectContext &ctx, const EngineRegisters &engine) {
        activeBindingDivisorDescs.clear();
        activeAttributeDescs.clear();

        for (size_t i{}; i < engine::VertexStreamCount; i++) {
            if (bindingDescs[i].inputRate == vk::VertexInputRate::eInstance) {
                if (!ctx.gpu.traits.supportsVertexAttributeDivisor) [[unlikely]]
                        Logger::Warn("Vertex attribute divisor used on guest without host support");
                else if (!ctx.gpu.traits.supportsVertexAttributeZeroDivisor && bindingDivisorDescs[i].divisor == 0) [[unlikely]]
                        Logger::Warn("Vertex attribute zero divisor used on guest without host support");
                else
                    activeBindingDivisorDescs.push_back(bindingDivisorDescs[i]);
            }
        }

        // TODO: check shader inputs
        for (size_t i{}; i < engine::VertexAttributeCount; i++)
            if (engine.vertexAttributesRegisters[i].source == engine::VertexAttribute::Source::Active)
                activeAttributeDescs.push_back(attributeDescs[i]);

        vk::StructureChain<vk::PipelineVertexInputStateCreateInfo, vk::PipelineVertexInputDivisorStateCreateInfoEXT> chain{
            vk::PipelineVertexInputStateCreateInfo{
                .vertexBindingDescriptionCount = static_cast<u32>(bindingDescs.size()),
                .pVertexBindingDescriptions = bindingDescs.data(),
                .vertexAttributeDescriptionCount = static_cast<u32>(activeAttributeDescs.size()),
                .pVertexAttributeDescriptions = activeAttributeDescs.data(),
            },
            vk::PipelineVertexInputDivisorStateCreateInfoEXT{
                .vertexBindingDivisorCount = static_cast<u32>(activeBindingDivisorDescs.size()),
                .pVertexBindingDivisors = activeBindingDivisorDescs.data(),
            },
        };

        if (activeBindingDivisorDescs.empty())
            chain.unlink<vk::PipelineVertexInputDivisorStateCreateInfoEXT>();

        return chain;
    }

    void VertexInputState::SetStride(u32 index, u32 stride) {
        bindingDescs[index].stride = stride;
    }

    void VertexInputState::SetInputRate(u32 index, engine::VertexStreamInstance instance) {
        bindingDescs[index].inputRate = instance.isInstanced ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex;
    }

    void VertexInputState::SetDivisor(u32 index, u32 divisor) {
        bindingDivisorDescs[index].divisor = divisor;
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

    static Shader::AttributeType ConvertShaderGenericInputType(engine::VertexAttribute::NumericalType numericalType) {
        using MaxwellType = engine::VertexAttribute::NumericalType;
        switch (numericalType) {
            case MaxwellType::Snorm:
            case MaxwellType::Unorm:
            case MaxwellType::Uscaled:
            case MaxwellType::Sscaled:
            case MaxwellType::Float:
                return Shader::AttributeType::Float;
            case MaxwellType::Sint:
                return Shader::AttributeType::SignedInt;
            case MaxwellType::Uint:
                return Shader::AttributeType::UnsignedInt;
            default:
                Logger::Warn("Unimplemented attribute type: {}", static_cast<u8>(numericalType));
                return Shader::AttributeType::Disabled;
        }
    }

    void VertexInputState::SetAttribute(u32 index, engine::VertexAttribute attribute) {
        auto &vkAttribute{attributeDescs[index]};
        if (attribute.source == engine::VertexAttribute::Source::Active) {
            vkAttribute.binding = attribute.stream;
            vkAttribute.format = ConvertVertexInputAttributeFormat(attribute.componentBitWidths, attribute.numericalType);
            vkAttribute.offset = attribute.offset;


            //  UpdateRuntimeInformation(runtimeInfo.generic_input_types[index], ConvertShaderGenericInputType(attribute.numericalType), maxwell3d::PipelineStage::Vertex);
        } else {
            //  UpdateRuntimeInformation(runtimeInfo.generic_input_types[index], Shader::AttributeType::Disabled, maxwell3d::PipelineStage::Vertex);
        }
    }
    
    /* Input Assembly State */
    void InputAssemblyState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, primitiveRestartEnable);
    }

    const vk::PipelineInputAssemblyStateCreateInfo &InputAssemblyState::Build() {
        return inputAssemblyState;
    }
    
    static std::pair<vk::PrimitiveTopology, Shader::InputTopology> ConvertPrimitiveTopology(engine::DrawTopology topology) {
        switch (topology) {
            case engine::DrawTopology::Points:
                return {vk::PrimitiveTopology::ePointList, Shader::InputTopology::Points};
            case engine::DrawTopology::Lines:
                return {vk::PrimitiveTopology::eLineList, Shader::InputTopology::Lines};
            case engine::DrawTopology::LineStrip:
                return {vk::PrimitiveTopology::eLineStrip, Shader::InputTopology::Lines};
            case engine::DrawTopology::Triangles:
                return {vk::PrimitiveTopology::eTriangleList, Shader::InputTopology::Triangles};
            case engine::DrawTopology::TriangleStrip:
                return {vk::PrimitiveTopology::eTriangleStrip, Shader::InputTopology::Triangles};
            case engine::DrawTopology::TriangleFan:
                return {vk::PrimitiveTopology::eTriangleFan, Shader::InputTopology::Triangles};
            case engine::DrawTopology::Quads:
                return {vk::PrimitiveTopology::eTriangleList, Shader::InputTopology::Triangles}; // Will use quad conversion
            case engine::DrawTopology::LineListAdjcy:
                return {vk::PrimitiveTopology::eLineListWithAdjacency, Shader::InputTopology::Lines};
            case engine::DrawTopology::LineStripAdjcy:
                return {vk::PrimitiveTopology::eLineStripWithAdjacency, Shader::InputTopology::Lines};
            case engine::DrawTopology::TriangleListAdjcy:
                return {vk::PrimitiveTopology::eTriangleListWithAdjacency, Shader::InputTopology::Triangles};
            case engine::DrawTopology::TriangleStripAdjcy:
                return {vk::PrimitiveTopology::eTriangleStripWithAdjacency, Shader::InputTopology::Triangles};
            case engine::DrawTopology::Patch:
                return {vk::PrimitiveTopology::ePatchList, Shader::InputTopology::Triangles};
            default:
                Logger::Warn("Unimplemented input assembly topology: {}", static_cast<u8>(topology));
                return {vk::PrimitiveTopology::eTriangleList, Shader::InputTopology::Triangles};
        }
    }

    void InputAssemblyState::SetPrimitiveTopology(engine::DrawTopology topology) {
        currentEngineTopology = topology;

        Shader::InputTopology geometryTopology{};
        std::tie(inputAssemblyState.topology, geometryTopology) = ConvertPrimitiveTopology(topology);

        /*
            if (shaderTopology == ShaderCompiler::InputTopology::Points)
                UpdateRuntimeInformation(runtimeInfo.fixed_state_point_size, std::make_optional(pointSpriteSize), maxwell3d::PipelineStage::Vertex, maxwell3d::PipelineStage::Geometry);
            else if (runtimeInfo.input_topology == ShaderCompiler::InputTopology::Points)
                UpdateRuntimeInformation(runtimeInfo.fixed_state_point_size, std::optional<float>{}, maxwell3d::PipelineStage::Vertex, maxwell3d::PipelineStage::Geometry);

            UpdateRuntimeInformation(runtimeInfo.input_topology, shaderTopology, maxwell3d::PipelineStage::Geometry);
         */
    }

    engine::DrawTopology InputAssemblyState::GetPrimitiveTopology() const {
        return currentEngineTopology;
    }

    bool InputAssemblyState::NeedsQuadConversion() const {
        return currentEngineTopology == engine::DrawTopology::Quads;
    }

    void InputAssemblyState::SetPrimitiveRestart(bool enabled) {
        inputAssemblyState.primitiveRestartEnable = enabled;
    }

    /* Tessellation State */
    void TessellationState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, patchControlPoints, tessellationParameters);
    }

    const vk::PipelineTessellationStateCreateInfo &TessellationState::Build() {
        return tessellationState;
    }

    void TessellationState::SetPatchControlPoints(u32 patchControlPoints) {
        tessellationState.patchControlPoints = patchControlPoints;
    }

    Shader::TessPrimitive ConvertShaderTessPrimitive(engine::TessellationParameters::DomainType domainType) {
        switch (domainType) {
            case engine::TessellationParameters::DomainType::Isoline:
                return Shader::TessPrimitive::Isolines;
            case engine::TessellationParameters::DomainType::Triangle:
                return Shader::TessPrimitive::Triangles;
            case engine::TessellationParameters::DomainType::Quad:
                return Shader::TessPrimitive::Quads;
        }
    }

    Shader::TessSpacing ConvertShaderTessSpacing(engine::TessellationParameters::Spacing spacing) {
        switch (spacing) {
            case engine::TessellationParameters::Spacing::Integer:
                return Shader::TessSpacing::Equal;
            case engine::TessellationParameters::Spacing::FractionalEven:
                return Shader::TessSpacing::FractionalEven;
            case engine::TessellationParameters::Spacing::FractionalOdd:
                return Shader::TessSpacing::FractionalOdd;
        }
    }

    void TessellationState::SetParameters(engine::TessellationParameters params) {
        // UpdateRuntimeInformation(runtimeInfo.tess_primitive, ConvertShaderTessPrimitive(params.domainType), maxwell3d::PipelineStage::TessellationEvaluation);
        // UpdateRuntimeInformation(runtimeInfo.tess_spacing, ConvertShaderTessSpacing(params.spacing), maxwell3d::PipelineStage::TessellationEvaluation);
        // UpdateRuntimeInformation(runtimeInfo.tess_clockwise, params.outputPrimitive == engine::TessellationParameters::OutputPrimitives::TrianglesCW,
        //                          maxwell3d::PipelineStage::TessellationEvaluation);
    }

    /* Rasterizer State */
    void RasterizationState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, rasterEnable, frontPolygonMode, backPolygonMode, viewportClipControl, oglCullEnable, oglFrontFace, oglCullFace, windowOrigin, provokingVertex, polyOffset);
    }

    RasterizationState::RasterizationState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    static vk::PolygonMode ConvertPolygonMode(engine::PolygonMode mode) {
        switch (mode) {
            case engine::PolygonMode::Fill:
                return vk::PolygonMode::eFill;
            case engine::PolygonMode::Line:
                return vk::PolygonMode::eLine;
            case engine::PolygonMode::Point:
                return vk::PolygonMode::ePoint;
        }
    }

    static vk::CullModeFlags ConvertCullMode(engine::CullFace cullMode) {
        switch (cullMode) {
            case engine::CullFace::Front:
                return vk::CullModeFlagBits::eFront;
            case engine::CullFace::Back:
                return vk::CullModeFlagBits::eBack;
            case engine::CullFace::FrontAndBack:
                return vk::CullModeFlagBits::eFrontAndBack;
        }
    }


    bool ConvertDepthBiasEnable(engine::PolyOffset polyOffset, engine::PolygonMode polygonMode) {
        switch (polygonMode) {
            case engine::PolygonMode::Point:
                return polyOffset.pointEnable;
            case engine::PolygonMode::Line:
                return polyOffset.lineEnable;
            case engine::PolygonMode::Fill:
                return polyOffset.fillEnable;
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

    void RasterizationState::Flush() {
        auto &rasterizationCreateInfo{rasterizationState.get<vk::PipelineRasterizationStateCreateInfo>()};
        rasterizationCreateInfo.rasterizerDiscardEnable = !engine->rasterEnable;
        rasterizationCreateInfo.polygonMode = ConvertPolygonMode(engine->frontPolygonMode);
        if (engine->backPolygonMode != engine->frontPolygonMode)
            Logger::Warn("Non-matching polygon modes!");

        rasterizationCreateInfo.cullMode = engine->oglCullEnable ? ConvertCullMode(engine->oglCullFace) : vk::CullModeFlagBits::eNone;
        //                UpdateRuntimeInformation(runtimeInfo.y_negate, enabled, maxwell3d::PipelineStage::Vertex, maxwell3d::PipelineStage::Fragment);

        bool origFrontFaceClockwise{engine->oglFrontFace == engine::FrontFace::CW};
        rasterizationCreateInfo.frontFace = (engine->windowOrigin.flipY != origFrontFaceClockwise) ? vk::FrontFace::eClockwise : vk::FrontFace::eCounterClockwise;
        rasterizationCreateInfo.depthBiasEnable = ConvertDepthBiasEnable(engine->polyOffset, engine->frontPolygonMode);
        rasterizationState.get<vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT>().provokingVertexMode = ConvertProvokingVertex(engine->provokingVertex.value);
    }

    /* Depth Stencil State */
    void DepthStencilState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, depthTestEnable, depthWriteEnable, depthFunc, depthBoundsTestEnable, stencilTestEnable, twoSidedStencilTestEnable, stencilOps, stencilBack);
    }

    DepthStencilState::DepthStencilState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    static vk::CompareOp ConvertCompareFunc(engine::CompareFunc func) {
        switch (func) {
            case engine::CompareFunc::D3DNever:
            case engine::CompareFunc::OglNever:
                return vk::CompareOp::eNever;
            case engine::CompareFunc::D3DLess:
            case engine::CompareFunc::OglLess:
                return vk::CompareOp::eLess;
            case engine::CompareFunc::D3DEqual:
            case engine::CompareFunc::OglEqual:
                return vk::CompareOp::eEqual;
            case engine::CompareFunc::D3DLessEqual:
            case engine::CompareFunc::OglLEqual:
                return vk::CompareOp::eLessOrEqual;
            case engine::CompareFunc::D3DGreater:
            case engine::CompareFunc::OglGreater:
                return vk::CompareOp::eGreater;
            case engine::CompareFunc::D3DNotEqual:
            case engine::CompareFunc::OglNotEqual:
                return vk::CompareOp::eNotEqual;
            case engine::CompareFunc::D3DGreaterEqual:
            case engine::CompareFunc::OglGEqual:
                return vk::CompareOp::eGreaterOrEqual;
            case engine::CompareFunc::D3DAlways:
            case engine::CompareFunc::OglAlways:
                return vk::CompareOp::eAlways;
        }
    }

    static vk::StencilOp ConvertStencilOp(engine::StencilOps::Op op) {
        switch (op) {
            case engine::StencilOps::Op::OglZero:
            case engine::StencilOps::Op::D3DZero:
                return vk::StencilOp::eZero;
            case engine::StencilOps::Op::D3DKeep:
            case engine::StencilOps::Op::OglKeep:
                return vk::StencilOp::eKeep;
            case engine::StencilOps::Op::D3DReplace:
            case engine::StencilOps::Op::OglReplace:
                return vk::StencilOp::eReplace;
            case engine::StencilOps::Op::D3DIncrSat:
            case engine::StencilOps::Op::OglIncrSat:
                return vk::StencilOp::eIncrementAndClamp;
            case engine::StencilOps::Op::D3DDecrSat:
            case engine::StencilOps::Op::OglDecrSat:
                return vk::StencilOp::eDecrementAndClamp;
            case engine::StencilOps::Op::D3DInvert:
            case engine::StencilOps::Op::OglInvert:
                return vk::StencilOp::eInvert;
            case engine::StencilOps::Op::D3DIncr:
            case engine::StencilOps::Op::OglIncr:
                return vk::StencilOp::eIncrementAndWrap;
            case engine::StencilOps::Op::D3DDecr:
            case engine::StencilOps::Op::OglDecr:
                return vk::StencilOp::eDecrementAndWrap;
        }
    }

    static vk::StencilOpState ConvertStencilOpsState(engine::StencilOps ops) {
        return {
            .passOp = ConvertStencilOp(ops.zPass),
            .depthFailOp = ConvertStencilOp(ops.zFail),
            .failOp = ConvertStencilOp(ops.fail),
            .compareOp = ConvertCompareFunc(ops.func),
        };
    }

    void DepthStencilState::Flush() {
        depthStencilState.depthTestEnable = engine->depthTestEnable;
        depthStencilState.depthWriteEnable = engine->depthWriteEnable;
        depthStencilState.depthCompareOp = ConvertCompareFunc(engine->depthFunc);
        depthStencilState.depthBoundsTestEnable = engine->depthBoundsTestEnable;
        depthStencilState.stencilTestEnable = engine->stencilTestEnable;

        auto stencilBack{engine->twoSidedStencilTestEnable ? engine->stencilBack : engine->stencilOps};
        depthStencilState.front = ConvertStencilOpsState(engine->stencilOps);
        depthStencilState.back = ConvertStencilOpsState(stencilBack);
    };

    /* Color Blend State */
    void ColorBlendState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, logicOp, singleCtWriteControl, ctWrites, blendStatePerTargetEnable, blendPerTargets, blend);
    }

    ColorBlendState::ColorBlendState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    vk::LogicOp ConvertLogicOpFunc(engine::LogicOp::Func func) {
        switch (func) {
            case engine::LogicOp::Func::Clear:
                return vk::LogicOp::eClear;
            case engine::LogicOp::Func::And:
                return vk::LogicOp::eAnd;
            case engine::LogicOp::Func::AndReverse:
                return vk::LogicOp::eAndReverse;
            case engine::LogicOp::Func::Copy:
                return vk::LogicOp::eCopy;
            case engine::LogicOp::Func::AndInverted:
                return vk::LogicOp::eAndInverted;
            case engine::LogicOp::Func::Noop:
                return vk::LogicOp::eNoOp;
            case engine::LogicOp::Func::Xor:
                return vk::LogicOp::eXor;
            case engine::LogicOp::Func::Or:
                return vk::LogicOp::eOr;
            case engine::LogicOp::Func::Nor:
                return vk::LogicOp::eNor;
            case engine::LogicOp::Func::Equiv:
                return vk::LogicOp::eEquivalent;
            case engine::LogicOp::Func::Invert:
                return vk::LogicOp::eInvert;
            case engine::LogicOp::Func::OrReverse:
                return vk::LogicOp::eOrReverse;
            case engine::LogicOp::Func::CopyInverted:
                return vk::LogicOp::eCopyInverted;
            case engine::LogicOp::Func::OrInverted:
                return vk::LogicOp::eOrInverted;
            case engine::LogicOp::Func::Nand:
                return vk::LogicOp::eNand;
            case engine::LogicOp::Func::Set:
                return vk::LogicOp::eSet;
            default:
                throw exception("Invalid logical operation type: 0x{:X}", static_cast<u32>(func));
        }
    }

    static vk::ColorComponentFlags ConvertColorWriteMask(engine::CtWrite write) {
        return vk::ColorComponentFlags{
            write.rEnable ? vk::ColorComponentFlagBits::eR : vk::ColorComponentFlags{} |
                write.gEnable ? vk::ColorComponentFlagBits::eG : vk::ColorComponentFlags{} |
                write.bEnable ? vk::ColorComponentFlagBits::eB : vk::ColorComponentFlags{} |
                write.aEnable ? vk::ColorComponentFlagBits::eA : vk::ColorComponentFlags{}
        };
    }

    static vk::BlendOp ConvertBlendOp(engine::BlendOp op) {
        switch (op) {
            case engine::BlendOp::D3DAdd:
            case engine::BlendOp::OglFuncAdd:
                return vk::BlendOp::eAdd;
            case engine::BlendOp::D3DSubtract:
            case engine::BlendOp::OglFuncSubtract:
                return vk::BlendOp::eSubtract;
            case engine::BlendOp::D3DRevSubtract:
            case engine::BlendOp::OglFuncReverseSubtract:
                return vk::BlendOp::eReverseSubtract;
            case engine::BlendOp::D3DMin:
            case engine::BlendOp::OglMin:
                return vk::BlendOp::eMin;
            case engine::BlendOp::D3DMax:
            case engine::BlendOp::OglMax:
                return vk::BlendOp::eMax;
            default:
                throw exception("Invalid blend operation: 0x{:X}", static_cast<u32>(op));
        }
    }

    static vk::BlendFactor ConvertBlendFactor(engine::BlendCoeff coeff) {
        switch (coeff) {
            case engine::BlendCoeff::OglZero:
            case engine::BlendCoeff::D3DZero:
                return vk::BlendFactor::eZero;
            case engine::BlendCoeff::OglOne:
            case engine::BlendCoeff::D3DOne:
                return vk::BlendFactor::eOne;
            case engine::BlendCoeff::OglSrcColor:
            case engine::BlendCoeff::D3DSrcColor:
                return vk::BlendFactor::eSrcColor;
            case engine::BlendCoeff::OglOneMinusSrcColor:
            case engine::BlendCoeff::D3DInvSrcColor:
                return vk::BlendFactor::eOneMinusSrcColor;
            case engine::BlendCoeff::OglSrcAlpha:
            case engine::BlendCoeff::D3DSrcAlpha:
                return vk::BlendFactor::eSrcAlpha;
            case engine::BlendCoeff::OglOneMinusSrcAlpha:
            case engine::BlendCoeff::D3DInvSrcAlpha:
                return vk::BlendFactor::eOneMinusSrcAlpha;
            case engine::BlendCoeff::OglDstAlpha:
            case engine::BlendCoeff::D3DDstAlpha:
                return vk::BlendFactor::eDstAlpha;
            case engine::BlendCoeff::OglOneMinusDstAlpha:
            case engine::BlendCoeff::D3DInvDstAlpha:
                return vk::BlendFactor::eOneMinusDstAlpha;
            case engine::BlendCoeff::OglDstColor:
            case engine::BlendCoeff::D3DDstColor:
                return vk::BlendFactor::eDstColor;
            case engine::BlendCoeff::OglOneMinusDstColor:
            case engine::BlendCoeff::D3DInvDstColor:
                return vk::BlendFactor::eOneMinusDstColor;
            case engine::BlendCoeff::OglSrcAlphaSaturate:
            case engine::BlendCoeff::D3DSrcAlphaSaturate:
                return vk::BlendFactor::eSrcAlphaSaturate;
            case engine::BlendCoeff::OglConstantColor:
            case engine::BlendCoeff::D3DBlendCoeff:
                return vk::BlendFactor::eConstantColor;
            case engine::BlendCoeff::OglOneMinusConstantColor:
            case engine::BlendCoeff::D3DInvBlendCoeff:
                return vk::BlendFactor::eOneMinusConstantColor;
            case engine::BlendCoeff::OglConstantAlpha:
                return vk::BlendFactor::eConstantAlpha;
            case engine::BlendCoeff::OglOneMinusConstantAlpha:
                return vk::BlendFactor::eOneMinusConstantAlpha;
            case engine::BlendCoeff::OglSrc1Color:
            case engine::BlendCoeff::D3DSrc1Color:
                return vk::BlendFactor::eSrc1Color;
            case engine::BlendCoeff::OglInvSrc1Color:
            case engine::BlendCoeff::D3DInvSrc1Color:
                return vk::BlendFactor::eOneMinusSrc1Color;
            case engine::BlendCoeff::OglSrc1Alpha:
            case engine::BlendCoeff::D3DSrc1Alpha:
                return vk::BlendFactor::eSrc1Alpha;
            case engine::BlendCoeff::OglInvSrc1Alpha:
            case engine::BlendCoeff::D3DInvSrc1Alpha:
                return vk::BlendFactor::eOneMinusSrc1Alpha;
            default:
                throw exception("Invalid blend coefficient type: 0x{:X}", static_cast<u32>(coeff));
        }
    }

    void ColorBlendState::Flush(InterconnectContext &ctx, size_t attachmentCount) {
        if (engine->logicOp.enable) {
            if (ctx.gpu.traits.supportsLogicOp) {
                colorBlendState.logicOpEnable = true;
                colorBlendState.logicOp = ConvertLogicOpFunc(engine->logicOp.func);
            } else {
                Logger::Warn("Cannot enable framebuffer logical operation without host GPU support!");
            }
        }

        auto convertBlendState{[](vk::PipelineColorBlendAttachmentState &attachmentBlendState, const auto &blend) {
            attachmentBlendState.colorBlendOp = ConvertBlendOp(blend.colorOp);
            attachmentBlendState.srcColorBlendFactor = ConvertBlendFactor(blend.colorSourceCoeff);
            attachmentBlendState.dstColorBlendFactor = ConvertBlendFactor(blend.colorDestCoeff);
            attachmentBlendState.alphaBlendOp = ConvertBlendOp(blend.alphaOp);
            attachmentBlendState.srcAlphaBlendFactor = ConvertBlendFactor(blend.alphaSourceCoeff);
            attachmentBlendState.dstAlphaBlendFactor = ConvertBlendFactor(blend.alphaDestCoeff);
        }};

        for (size_t i{}; i < engine::ColorTargetCount; i++) {
            auto &attachmentBlendState{attachmentBlendStates[i]};
            attachmentBlendState.blendEnable = engine->blend.enable[i];
            attachmentBlendState.colorWriteMask = ConvertColorWriteMask(engine->singleCtWriteControl ? engine->ctWrites[0] : engine->ctWrites[i]);
            if (engine->blendStatePerTargetEnable)
                convertBlendState(attachmentBlendState, engine->blendPerTargets[i]);
            else
                convertBlendState(attachmentBlendState, engine->blend);
        }

        colorBlendState.attachmentCount = static_cast<u32>(attachmentCount);
        colorBlendState.pAttachments = attachmentBlendStates.data();
    }

    void ColorBlendState::Refresh(InterconnectContext &ctx, size_t attachmentCount) {
        colorBlendState.attachmentCount = static_cast<u32>(attachmentCount);
    }

    /* Pipeline State */
    void PipelineState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        auto bindFunc{[&](auto &regs) { regs.DirtyBind(manager, handle); }};

        ranges::for_each(colorRenderTargetsRegisters, bindFunc);
        bindFunc(depthRenderTargetRegisters);
        bindFunc(vertexInputRegisters);
    }

    PipelineState::PipelineState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine)
        : engine{manager, dirtyHandle, engine},
          colorRenderTargets{util::MergeInto<dirty::ManualDirtyState<ColorRenderTargetState>, engine::ColorTargetCount>(manager, engine.colorRenderTargetsRegisters)},
          depthRenderTarget{manager, engine.depthRenderTargetRegisters},
          rasterization{manager, engine.rasterizationRegisters},
          depthStencil{manager, engine.depthStencilRegisters},
          colorBlend{manager, engine.colorBlendRegisters} {}

    void PipelineState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder) {
        boost::container::static_vector<TextureView *, engine::ColorTargetCount> colorAttachments;
        for (auto &colorRenderTarget : colorRenderTargets)
            if (auto view{colorRenderTarget.UpdateGet(ctx).view}; view)
                colorAttachments.push_back(view.get());

        TextureView *depthAttachment{depthRenderTarget.UpdateGet(ctx).view.get()};

        auto vertexInputState{directState.vertexInput.Build(ctx, engine->vertexInputRegisters)};
        const auto &inputAssemblyState{directState.inputAssembly.Build()};
        const auto &tessellationState{directState.tessellation.Build()};
        const auto &rasterizationState{rasterization.UpdateGet().rasterizationState};
        vk::PipelineMultisampleStateCreateInfo multisampleState{
            .rasterizationSamples = vk::SampleCountFlagBits::e1
        };
        const auto &depthStencilState{depthStencil.UpdateGet().depthStencilState};
        const auto &colorBlendState{colorBlend.UpdateGet(ctx, colorAttachments.size()).colorBlendState};
    }

    std::shared_ptr<TextureView> PipelineState::GetColorRenderTargetForClear(InterconnectContext &ctx, size_t index) {
        return colorRenderTargets[index].UpdateGet(ctx).view;
    }

    std::shared_ptr<TextureView> PipelineState::GetDepthRenderTargetForClear(InterconnectContext &ctx) {
        return depthRenderTarget.UpdateGet(ctx).view;
    }
}