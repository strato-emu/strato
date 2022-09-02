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
          depthRenderTarget{manager, engine.depthRenderTargetRegisters} {}

    void PipelineState::Flush(InterconnectContext &ctx, StateUpdateBuilder &builder) {
        auto updateFunc{[&](auto &stateElem, auto &&... args) { stateElem.Update(ctx, args...); }};
        ranges::for_each(colorRenderTargets, updateFunc);
        updateFunc(depthRenderTarget);

        auto vertexState{directState.vertexInput.Build(ctx, engine->vertexInputRegisters)};
        auto inputAssemblyState{directState.inputAssembly.Build()};

    }

    std::shared_ptr<TextureView> PipelineState::GetColorRenderTargetForClear(InterconnectContext &ctx, size_t index) {
        return colorRenderTargets[index].UpdateGet(ctx).view;
    }

    std::shared_ptr<TextureView> PipelineState::GetDepthRenderTargetForClear(InterconnectContext &ctx) {
        return depthRenderTarget.UpdateGet(ctx).view;
    }
}