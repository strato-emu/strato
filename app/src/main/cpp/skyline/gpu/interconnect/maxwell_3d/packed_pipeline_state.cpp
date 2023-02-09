// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/Ryujinx/)
// Copyright © 2022 yuzu Team and Contributors (https://github.com/yuzu-emu/)
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <range/v3/algorithm.hpp>
#include "packed_pipeline_state.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wbitfield-enum-conversion"

namespace skyline::gpu::interconnect::maxwell3d {
    static constexpr u8 DepthDisabledMagic{0x1f}; // Format value (unused in HW) used to signal that depth is disabled

    void PackedPipelineState::SetColorRenderTargetFormat(size_t index, engine::ColorTarget::Format format) {
        colorRenderTargetFormats[index] = static_cast<u8>(format);
    }

    void PackedPipelineState::SetDepthRenderTargetFormat(engine::ZtFormat format, bool enabled) {
        if (enabled)
            depthRenderTargetFormat = static_cast<u8>(format) - static_cast<u8>(engine::ZtFormat::ZF32);
        else
            depthRenderTargetFormat = DepthDisabledMagic;
    }

    void PackedPipelineState::SetVertexBinding(u32 index, engine::VertexStream stream, engine::VertexStreamInstance instance) {
        if (!dynamicStateActive)
            vertexStrides[index] = stream.format.stride;
        vertexBindings[index].inputRate = static_cast<u8>(instance.isInstanced ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex);
        vertexBindings[index].enable = stream.format.enable;
        vertexBindings[index].divisor = stream.frequency;
    }

    void PackedPipelineState::SetTessellationParameters(engine::TessellationParameters parameters) {
        domainType = parameters.domainType;
        spacing = parameters.spacing;
        outputPrimitives = parameters.outputPrimitives;
    }

    void PackedPipelineState::SetPolygonMode(engine::PolygonMode mode) {
        switch (mode) {
            case engine::PolygonMode::Fill:
                polygonMode = static_cast<u8>(vk::PolygonMode::eFill);
                break;
            case engine::PolygonMode::Line:
                polygonMode = static_cast<u8>(vk::PolygonMode::eLine);
                break;
            case engine::PolygonMode::Point:
                polygonMode = static_cast<u8>(vk::PolygonMode::ePoint);
                break;
            default:
                throw exception("Invalid polygon mode: 0x{:X}", static_cast<u32>(mode));
        }
    }

    vk::PolygonMode PackedPipelineState::GetPolygonMode() const {
        return static_cast<vk::PolygonMode>(polygonMode);
    }

    void PackedPipelineState::SetCullMode(bool enable, engine::CullFace mode) {
        if (!enable) {
            cullMode = {};
            return;
        }

        switch (mode) {
            case engine::CullFace::Front:
                cullMode = VK_CULL_MODE_FRONT_BIT;
                break;
            case engine::CullFace::Back:
                cullMode = VK_CULL_MODE_BACK_BIT;
                break;
            case engine::CullFace::FrontAndBack:
                cullMode = VK_CULL_MODE_FRONT_BIT | VK_CULL_MODE_BACK_BIT;
                break;
            default:
                throw exception("Invalid cull mode: 0x{:X}", static_cast<u32>(mode));
        }
    }

    static u8 ConvertCompareFunc(engine::CompareFunc func) {
        if (func < engine::CompareFunc::D3DNever || func > engine::CompareFunc::OglAlways || (func > engine::CompareFunc::D3DAlways && func < engine::CompareFunc::OglNever))
            throw exception("Invalid comparision function: 0x{:X}", static_cast<u32>(func));

        u32 val{static_cast<u32>(func)};

        // VK CompareOp values match 1:1 with Maxwell with some small maths
        return static_cast<u8>(func >= engine::CompareFunc::OglNever ? val - 0x200 : val - 1);
    }

    void PackedPipelineState::SetDepthFunc(engine::CompareFunc func) {
        depthFunc = ConvertCompareFunc(func);
    }

    vk::CompareOp PackedPipelineState::GetDepthFunc() const {
        return static_cast<vk::CompareOp>(depthFunc);
    }

    void PackedPipelineState::SetLogicOp(engine::LogicOp::Func op) {
        if (op < engine::LogicOp::Func::Clear || op > engine::LogicOp::Func::Set)
            throw exception("Invalid logical operation: 0x{:X}", static_cast<u32>(op));

        // VK LogicOp values match 1:1 with Maxwell
        logicOp = static_cast<u8>(static_cast<u32>(op) - static_cast<u32>(engine::LogicOp::Func::Clear));
    }

    vk::LogicOp PackedPipelineState::GetLogicOp() const {
        return static_cast<vk::LogicOp>(logicOp);
    }

    texture::Format PackedPipelineState::GetColorRenderTargetFormat(size_t rawIndex) const {
        auto format{static_cast<engine::ColorTarget::Format>(colorRenderTargetFormats[rawIndex])};
        #define FORMAT_CASE_BASE(engineFormat, skFormat, warn) \
                case engine::ColorTarget::Format::engineFormat:                     \
                    if constexpr (warn)                                             \
                        Logger::Warn("Partially supported RT format: " #engineFormat " used!"); \
                    return skyline::gpu::format::skFormat

        #define FORMAT_CASE(engineFormat, skFormat) FORMAT_CASE_BASE(engineFormat, skFormat, false)
        #define FORMAT_CASE_WARN(engineFormat, skFormat) FORMAT_CASE_BASE(engineFormat, skFormat, true)

        switch (format) {
            case engine::ColorTarget::Format::Disabled:
                return texture::Format{};
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
            FORMAT_CASE(AU8BU8GU8RU8, R8G8B8A8Uint);
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

    bool PackedPipelineState::IsColorRenderTargetEnabled(size_t rawIndex) const {
        return colorRenderTargetFormats[rawIndex] != 0;
    }

    size_t PackedPipelineState::GetColorRenderTargetCount() const {
        return ctSelect.count;
    }

    texture::Format PackedPipelineState::GetDepthRenderTargetFormat() const {
        if (depthRenderTargetFormat == DepthDisabledMagic)
            return texture::Format{};

        auto format{static_cast<engine::ZtFormat>(depthRenderTargetFormat + static_cast<u8>(engine::ZtFormat::ZF32))};
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

    static u8 ConvertStencilOp(engine::StencilOps::Op op) {
        auto conv{[&]() {
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
                default:
                    throw exception("Invalid stencil operation: 0x{:X}", static_cast<u32>(op));
            }
        }};

        return static_cast<u8>(conv());
    }

    static PackedPipelineState::StencilOps PackStencilOps(engine::StencilOps ops) {
        return {
            .zPass = ConvertStencilOp(ops.zPass),
            .fail = ConvertStencilOp(ops.fail),
            .zFail = ConvertStencilOp(ops.zFail),
            .func = ConvertCompareFunc(ops.func),
        };
    }

    void PackedPipelineState::SetStencilOps(engine::StencilOps front, engine::StencilOps back) {
        stencilFront = PackStencilOps(front);
        stencilBack = PackStencilOps(back);
    }

    static VkColorComponentFlags ConvertColorWriteMask(engine::CtWrite write) {
        return (write.rEnable ? VK_COLOR_COMPONENT_R_BIT : 0) |
            (write.gEnable ? VK_COLOR_COMPONENT_G_BIT : 0) |
            (write.bEnable ? VK_COLOR_COMPONENT_B_BIT : 0) |
            (write.aEnable ? VK_COLOR_COMPONENT_A_BIT : 0);
    };

    static u8 ConvertBlendOp(engine::BlendOp op) {
        auto conv{[&]() {
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
        }};

        return static_cast<u8>(conv());
    }

    static u8 ConvertBlendFactor(engine::BlendCoeff coeff) {
        auto conv{[&]() {
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
        }};

        return static_cast<u8>(conv());
    }

    static PackedPipelineState::AttachmentBlendState PackAttachmentBlendState(bool enable, engine::CtWrite writeMask, auto blend) {
        return {
            .colorWriteMask = ConvertColorWriteMask(writeMask),
            .colorBlendOp = enable ? ConvertBlendOp(blend.colorOp) : u8{0},
            .srcColorBlendFactor = enable ? ConvertBlendFactor(blend.colorSourceCoeff) : u8{0},
            .dstColorBlendFactor = enable ? ConvertBlendFactor(blend.colorDestCoeff) : u8{0},
            .alphaBlendOp = enable ? ConvertBlendOp(blend.alphaOp) : u8{0},
            .srcAlphaBlendFactor = enable ? ConvertBlendFactor(blend.alphaSourceCoeff) : u8{0},
            .dstAlphaBlendFactor = enable ? ConvertBlendFactor(blend.alphaDestCoeff) : u8{0},
            .blendEnable = enable
        };
    }

    void PackedPipelineState::SetAttachmentBlendState(u32 index, bool enable, engine::CtWrite writeMask, engine::Blend blend) {
        attachmentBlendStates[index] = PackAttachmentBlendState(enable, writeMask, blend);
    }

    void PackedPipelineState::SetAttachmentBlendState(u32 index, bool enable, engine::CtWrite writeMask, engine::BlendPerTarget blend) {
        attachmentBlendStates[index] = PackAttachmentBlendState(enable, writeMask, blend);
    }

    std::array<vk::StencilOpState, 2> PackedPipelineState::GetStencilOpsState() const {
        auto convertFaceOps{[](StencilOps ops) {
            return vk::StencilOpState{
                .failOp = static_cast<vk::StencilOp>(ops.fail),
                .passOp = static_cast<vk::StencilOp>(ops.zPass),
                .depthFailOp = static_cast<vk::StencilOp>(ops.zFail),
                .compareOp = static_cast<vk::CompareOp>(ops.func),
            };
        }};

        return {
            convertFaceOps(stencilFront),
            convertFaceOps(stencilBack)
        };
    }

    vk::PipelineColorBlendAttachmentState PackedPipelineState::GetAttachmentBlendState(u32 index) const {
        const auto &state{attachmentBlendStates[index]};

        return {
            .colorWriteMask = vk::ColorComponentFlags{state.colorWriteMask},
            .colorBlendOp = static_cast<vk::BlendOp>(state.colorBlendOp),
            .srcColorBlendFactor = static_cast<vk::BlendFactor>(state.srcColorBlendFactor),
            .dstColorBlendFactor = static_cast<vk::BlendFactor>(state.dstColorBlendFactor),
            .alphaBlendOp = static_cast<vk::BlendOp>(state.alphaBlendOp),
            .srcAlphaBlendFactor = static_cast<vk::BlendFactor>(state.srcAlphaBlendFactor),
            .dstAlphaBlendFactor = static_cast<vk::BlendFactor>(state.dstAlphaBlendFactor),
            .blendEnable = state.blendEnable
        };
    }

    void PackedPipelineState::SetTransformFeedbackVaryings(const engine::StreamOutControl &control, const std::array<u8, engine::StreamOutLayoutSelectAttributeCount> &layoutSelect, size_t buffer) {
        if (control.streamSelect != 0)
            throw exception("Geometry streams are unsupported!");

        for (size_t i{}; i < control.componentCount; i++) {
            // TODO: We could merge multiple component accesses from the same attribute into one varying as yuzu does
            u8 attributeIndex{layoutSelect[i]};

            if (control.strideBytes > std::numeric_limits<u16>::max())
                throw exception("Stride too large: {}", control.strideBytes);

            transformFeedbackVaryings[attributeIndex] = {
                .buffer = static_cast<u8>(buffer),
                .offsetWords = static_cast<u8>(i),
                .stride = static_cast<u16>(control.strideBytes),
                .valid = true
            };
        }
    }

    std::vector<Shader::TransformFeedbackVarying> PackedPipelineState::GetTransformFeedbackVaryings() const {
        std::vector<Shader::TransformFeedbackVarying> convertedVaryings;
        convertedVaryings.resize(0x100);
        ranges::transform(transformFeedbackVaryings, convertedVaryings.begin(), [](auto &varying) {
            if (varying.valid) {
                return Shader::TransformFeedbackVarying{
                    .buffer = varying.buffer,
                    .stride = varying.stride,
                    .offset = varying.offsetWords * 4U,
                    .components = 1,
                };
            } else {
                return Shader::TransformFeedbackVarying{};
            }
        });

        return convertedVaryings;
    }

    void PackedPipelineState::SetAlphaFunc(engine::CompareFunc func) {
        alphaFunc = ConvertCompareFunc(func);
    }

    Shader::CompareFunction PackedPipelineState::GetAlphaFunc() const {
        // Vulkan enum values match 1-1 with hades
        return static_cast<Shader::CompareFunction>(alphaFunc);
    }

    void PackedPipelineState::SetDepthClampEnable(engine::ViewportClipControl::GeometryClip clip) {
        depthClampEnable = (clip != engine::ViewportClipControl::GeometryClip::Passthru) && (clip != engine::ViewportClipControl::GeometryClip::FrustrumXYZClip) && (clip != engine::ViewportClipControl::GeometryClip::FrustrumZClip);
    }
}

#pragma clang diagnostic pop