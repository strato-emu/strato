// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#include <boost/preprocessor/repeat.hpp>
#include <soc.h>
#include "maxwell_3d.h"

namespace skyline::soc::gm20b::engine::maxwell3d {
    Maxwell3D::Maxwell3D(const DeviceState &state, ChannelContext &channelCtx, MacroState &macroState, gpu::interconnect::CommandExecutor &executor)
        : MacroEngineBase(macroState),
          syncpoints(state.soc->host1x.syncpoints),
          context(*state.gpu, channelCtx, executor),
          channelCtx(channelCtx) {
        InitializeRegisters();
    }

    void Maxwell3D::CallMethodFromMacro(u32 method, u32 argument) {
        HandleMethod(method, argument);
    }

    u32 Maxwell3D::ReadMethodFromMacro(u32 method) {
        return registers.raw[method];
    }

    __attribute__((always_inline)) void Maxwell3D::CallMethod(u32 method, u32 argument) {
        Logger::Verbose("Called method in Maxwell 3D: 0x{:X} args: 0x{:X}", method, argument);

        HandleMethod(method, argument);
    }

    void Maxwell3D::HandleMethod(u32 method, u32 argument) {
        #define MAXWELL3D_OFFSET(field) (sizeof(typeof(Registers::field)) - sizeof(std::remove_reference_t<decltype(*Registers::field)>)) / sizeof(u32)
        #define MAXWELL3D_STRUCT_OFFSET(field, member) MAXWELL3D_OFFSET(field) + U32_OFFSET(std::remove_reference_t<decltype(*Registers::field)>, member)
        #define MAXWELL3D_STRUCT_STRUCT_OFFSET(field, member, submember) MAXWELL3D_STRUCT_OFFSET(field, member) + U32_OFFSET(std::remove_reference_t<decltype(Registers::field->member)>, submember)
        #define MAXWELL3D_STRUCT_ARRAY_OFFSET(field, member, index) MAXWELL3D_STRUCT_OFFSET(field, member) + ((sizeof(std::remove_reference_t<decltype(Registers::field->member[0])>) / sizeof(u32)) * index)
        #define MAXWELL3D_ARRAY_OFFSET(field, index) MAXWELL3D_OFFSET(field) + ((sizeof(std::remove_reference_t<decltype(Registers::field[0])>) / sizeof(u32)) * index)
        #define MAXWELL3D_ARRAY_STRUCT_OFFSET(field, index, member) MAXWELL3D_ARRAY_OFFSET(field, index) + U32_OFFSET(std::remove_reference_t<decltype(Registers::field[0])>, member)
        #define MAXWELL3D_ARRAY_STRUCT_STRUCT_OFFSET(field, index, member, submember) MAXWELL3D_ARRAY_STRUCT_OFFSET(field, index, member) + U32_OFFSET(decltype(Registers::field[0].member), submember)

        #define MAXWELL3D_CASE(field, content) case MAXWELL3D_OFFSET(field): {                        \
            auto field{util::BitCast<std::remove_reference_t<decltype(*registers.field)>>(argument)}; \
            content                                                                                   \
            return;                                                                                   \
        }
        #define MAXWELL3D_CASE_BASE(fieldName, fieldAccessor, offset, content) case offset: {                    \
            auto fieldName{util::BitCast<std::remove_reference_t<decltype(registers.fieldAccessor)>>(argument)}; \
            content                                                                                              \
            return;                                                                                              \
        }
        #define MAXWELL3D_STRUCT_CASE(field, member, content) MAXWELL3D_CASE_BASE(member, field->member, MAXWELL3D_STRUCT_OFFSET(field, member), content)
        #define MAXWELL3D_STRUCT_STRUCT_CASE(field, member, submember, content) MAXWELL3D_CASE_BASE(submember, field->member.submember, MAXWELL3D_STRUCT_STRUCT_OFFSET(field, member, submember), content)
        #define MAXWELL3D_STRUCT_ARRAY_CASE(field, member, index, content) MAXWELL3D_CASE_BASE(member, field->member[index], MAXWELL3D_STRUCT_ARRAY_OFFSET(field, member, index), content)
        #define MAXWELL3D_ARRAY_CASE(field, index, content) MAXWELL3D_CASE_BASE(field, field[index], MAXWELL3D_ARRAY_OFFSET(field, index), content)
        #define MAXWELL3D_ARRAY_STRUCT_CASE(field, index, member, content) MAXWELL3D_CASE_BASE(member, field[index].member, MAXWELL3D_ARRAY_STRUCT_OFFSET(field, index, member), content)
        #define MAXWELL3D_ARRAY_STRUCT_STRUCT_CASE(field, index, member, submember, content) MAXWELL3D_CASE_BASE(submember, field[index].member.submember, MAXWELL3D_ARRAY_STRUCT_STRUCT_OFFSET(field, index, member, submember), content)

        if (method != MAXWELL3D_STRUCT_OFFSET(mme, shadowRamControl)) {
            if (shadowRegisters.mme->shadowRamControl == type::MmeShadowRamControl::MethodTrack || shadowRegisters.mme->shadowRamControl == type::MmeShadowRamControl::MethodTrackWithFilter)
                shadowRegisters.raw[method] = argument;
            else if (shadowRegisters.mme->shadowRamControl == type::MmeShadowRamControl::MethodReplay)
                argument = shadowRegisters.raw[method];
        }

        bool redundant{registers.raw[method] == argument};
        registers.raw[method] = argument;

        if (!redundant) {
            switch (method) {
                MAXWELL3D_STRUCT_CASE(mme, shadowRamControl, {
                    shadowRegisters.mme->shadowRamControl = shadowRamControl;
                })

                #define RENDER_TARGET_ARRAY(z, index, data)                               \
                MAXWELL3D_ARRAY_STRUCT_STRUCT_CASE(renderTargets, index, address, high, { \
                    context.SetColorRenderTargetAddressHigh(index, high);                 \
                })                                                                        \
                MAXWELL3D_ARRAY_STRUCT_STRUCT_CASE(renderTargets, index, address, low, {  \
                    context.SetColorRenderTargetAddressLow(index, low);                   \
                })                                                                        \
                MAXWELL3D_ARRAY_STRUCT_CASE(renderTargets, index, width, {                \
                    context.SetColorRenderTargetWidth(index, width);                      \
                })                                                                        \
                MAXWELL3D_ARRAY_STRUCT_CASE(renderTargets, index, height, {               \
                    context.SetColorRenderTargetHeight(index, height);                    \
                })                                                                        \
                MAXWELL3D_ARRAY_STRUCT_CASE(renderTargets, index, format, {               \
                    context.SetColorRenderTargetFormat(index, format);                    \
                })                                                                        \
                MAXWELL3D_ARRAY_STRUCT_CASE(renderTargets, index, tileMode, {             \
                    context.SetColorRenderTargetTileMode(index, tileMode);                \
                })                                                                        \
                MAXWELL3D_ARRAY_STRUCT_CASE(renderTargets, index, arrayMode, {            \
                    context.SetColorRenderTargetArrayMode(index, arrayMode);              \
                })                                                                        \
                MAXWELL3D_ARRAY_STRUCT_CASE(renderTargets, index, layerStrideLsr2, {      \
                    context.SetColorRenderTargetLayerStride(index, layerStrideLsr2);      \
                })                                                                        \
                MAXWELL3D_ARRAY_STRUCT_CASE(renderTargets, index, baseLayer, {            \
                    context.SetColorRenderTargetBaseLayer(index, baseLayer);              \
                })

                BOOST_PP_REPEAT(8, RENDER_TARGET_ARRAY, 0)
                static_assert(type::RenderTargetCount == 8 && type::RenderTargetCount < BOOST_PP_LIMIT_REPEAT);
                #undef RENDER_TARGET_ARRAY

                MAXWELL3D_CASE(depthTargetEnable, {
                    context.SetDepthRenderTargetEnabled(depthTargetEnable);
                })
                MAXWELL3D_STRUCT_CASE(depthTargetAddress, high, {
                    context.SetDepthRenderTargetAddressHigh(high);
                })
                MAXWELL3D_STRUCT_CASE(depthTargetAddress, low, {
                    context.SetDepthRenderTargetAddressLow(low);
                })
                MAXWELL3D_CASE(depthTargetFormat, {
                    context.SetDepthRenderTargetFormat(depthTargetFormat);
                })
                MAXWELL3D_CASE(depthTargetTileMode, {
                    context.SetDepthRenderTargetTileMode(depthTargetTileMode);
                })
                MAXWELL3D_CASE(depthTargetLayerStride, {
                    context.SetDepthRenderTargetLayerStride(depthTargetLayerStride);
                })
                MAXWELL3D_CASE(depthTargetWidth, {
                    context.SetDepthRenderTargetWidth(depthTargetWidth);
                })
                MAXWELL3D_CASE(depthTargetHeight, {
                    context.SetDepthRenderTargetHeight(depthTargetHeight);
                })
                MAXWELL3D_CASE(depthTargetArrayMode, {
                    context.SetDepthRenderTargetArrayMode(depthTargetArrayMode);
                })

                #define VIEWPORT_TRANSFORM_CALLBACKS(_z, index, data)                                      \
                MAXWELL3D_ARRAY_STRUCT_CASE(viewportTransforms, index, scaleX, {                          \
                    context.SetViewportX(index, scaleX, registers.viewportTransforms[index].translateX);  \
                })                                                                                        \
                MAXWELL3D_ARRAY_STRUCT_CASE(viewportTransforms, index, translateX, {                      \
                    context.SetViewportX(index, registers.viewportTransforms[index].scaleX, translateX);  \
                })                                                                                        \
                MAXWELL3D_ARRAY_STRUCT_CASE(viewportTransforms, index, scaleY, {                          \
                    context.SetViewportY(index, scaleY, registers.viewportTransforms[index].translateY);  \
                })                                                                                        \
                MAXWELL3D_ARRAY_STRUCT_CASE(viewportTransforms, index, translateY, {                      \
                    context.SetViewportY(index, registers.viewportTransforms[index].scaleY, translateY);  \
                })                                                                                        \
                MAXWELL3D_ARRAY_STRUCT_CASE(viewportTransforms, index, scaleZ, {                          \
                    context.SetViewportZ(index, scaleZ, registers.viewportTransforms[index].translateZ);  \
                })                                                                                        \
                MAXWELL3D_ARRAY_STRUCT_CASE(viewportTransforms, index, translateZ, {                      \
                    context.SetViewportZ(index, registers.viewportTransforms[index].scaleZ, translateZ);  \
                })                                                                                        \
                MAXWELL3D_ARRAY_STRUCT_CASE(viewportTransforms, index, swizzles, {                        \
                    context.SetViewportSwizzle(index, swizzles.x, swizzles.y, swizzles.z, swizzles.w);    \
                })

                BOOST_PP_REPEAT(16, VIEWPORT_TRANSFORM_CALLBACKS, 0)
                static_assert(type::ViewportCount == 16 && type::ViewportCount < BOOST_PP_LIMIT_REPEAT);
                #undef VIEWPORT_TRANSFORM_CALLBACKS

                #define COLOR_CLEAR_CALLBACKS(z, index, data)              \
                MAXWELL3D_ARRAY_CASE(clearColorValue, index, {             \
                    context.UpdateClearColorValue(index, clearColorValue); \
                })

                BOOST_PP_REPEAT(4, COLOR_CLEAR_CALLBACKS, 0)
                static_assert(4 < BOOST_PP_LIMIT_REPEAT);
                #undef COLOR_CLEAR_CALLBACKS

                MAXWELL3D_CASE(clearDepthValue, {
                    context.UpdateClearDepthValue(clearDepthValue);
                })

                MAXWELL3D_CASE(clearStencilValue, {
                    context.UpdateClearStencilValue(clearStencilValue);
                })

                MAXWELL3D_STRUCT_CASE(polygonMode, front, {
                    context.SetPolygonModeFront(front);
                })

                MAXWELL3D_STRUCT_CASE(depthBiasEnable, point, {
                    context.SetDepthBiasPointEnabled(point);
                })

                MAXWELL3D_STRUCT_CASE(depthBiasEnable, line, {
                    context.SetDepthBiasLineEnabled(line);
                })

                MAXWELL3D_STRUCT_CASE(depthBiasEnable, fill, {
                    context.SetDepthBiasFillEnabled(fill);
                })

                #define SCISSOR_CALLBACKS(z, index, data)                                                           \
                MAXWELL3D_ARRAY_STRUCT_CASE(scissors, index, enable, {                                              \
                    context.SetScissor(index, enable ? registers.scissors[index] : std::optional<type::Scissor>{}); \
                })                                                                                                  \
                MAXWELL3D_ARRAY_STRUCT_CASE(scissors, index, horizontal, {                                          \
                    context.SetScissorHorizontal(index, horizontal);                                                \
                })                                                                                                  \
                MAXWELL3D_ARRAY_STRUCT_CASE(scissors, index, vertical, {                                            \
                    context.SetScissorVertical(index, vertical);                                                    \
                })

                BOOST_PP_REPEAT(16, SCISSOR_CALLBACKS, 0)
                static_assert(type::ViewportCount == 16 && type::ViewportCount < BOOST_PP_LIMIT_REPEAT);
                #undef SCISSOR_CALLBACKS

                MAXWELL3D_CASE(commonColorWriteMask, {
                    if (commonColorWriteMask) {
                        auto colorWriteMask{registers.colorWriteMask[0]};
                        for (u32 index{}; index != type::RenderTargetCount; index++)
                            context.SetColorWriteMask(index, colorWriteMask);
                    } else {
                        for (u32 index{}; index != type::RenderTargetCount; index++)
                            context.SetColorWriteMask(index, registers.colorWriteMask[index]);
                    }
                })

                MAXWELL3D_CASE(renderTargetControl, {
                    context.UpdateRenderTargetControl(renderTargetControl);
                })

                MAXWELL3D_CASE(depthTestEnable, {
                    context.SetDepthTestEnabled(depthTestEnable);
                })

                MAXWELL3D_CASE(depthTestFunc, {
                    context.SetDepthTestFunction(depthTestFunc);
                })

                MAXWELL3D_CASE(depthWriteEnable, {
                    context.SetDepthWriteEnabled(depthWriteEnable);
                })

                MAXWELL3D_CASE(depthBoundsEnable, {
                    context.SetDepthBoundsTestEnabled(depthBoundsEnable);
                })

                MAXWELL3D_CASE(depthBoundsNear, {
                    context.SetMinDepthBounds(depthBoundsNear);
                })

                MAXWELL3D_CASE(depthBoundsFar, {
                    context.SetMaxDepthBounds(depthBoundsFar);
                })

                MAXWELL3D_CASE(stencilEnable, {
                    context.SetStencilTestEnabled(stencilEnable);
                })

                MAXWELL3D_STRUCT_CASE(stencilFront, failOp, {
                    context.SetStencilFrontFailOp(failOp);
                })

                MAXWELL3D_STRUCT_CASE(stencilFront, zFailOp, {
                    context.SetStencilFrontDepthFailOp(zFailOp);
                })

                MAXWELL3D_STRUCT_CASE(stencilFront, passOp, {
                    context.SetStencilFrontPassOp(passOp);
                })

                MAXWELL3D_STRUCT_CASE(stencilFront, compareOp, {
                    context.SetStencilFrontCompareOp(compareOp);
                })

                MAXWELL3D_STRUCT_CASE(stencilFront, compareReference, {
                    context.SetStencilFrontReference(compareReference);
                })

                MAXWELL3D_STRUCT_CASE(stencilFront, compareMask, {
                    context.SetStencilFrontCompareMask(compareMask);
                })

                MAXWELL3D_STRUCT_CASE(stencilFront, writeMask, {
                    context.SetStencilFrontWriteMask(writeMask);
                })

                MAXWELL3D_STRUCT_CASE(stencilBack, failOp, {
                    context.SetStencilBackFailOp(failOp);
                })

                MAXWELL3D_STRUCT_CASE(stencilBack, zFailOp, {
                    context.SetStencilBackDepthFailOp(zFailOp);
                })

                MAXWELL3D_STRUCT_CASE(stencilBack, passOp, {
                    context.SetStencilBackPassOp(passOp);
                })

                MAXWELL3D_STRUCT_CASE(stencilBack, compareOp, {
                    context.SetStencilBackCompareOp(compareOp);
                })

                MAXWELL3D_STRUCT_CASE(stencilBackExtra, compareReference, {
                    context.SetStencilBackReference(compareReference);
                })

                MAXWELL3D_STRUCT_CASE(stencilBackExtra, compareMask, {
                    context.SetStencilBackCompareMask(compareMask);
                })

                MAXWELL3D_STRUCT_CASE(stencilBackExtra, writeMask, {
                    context.SetStencilBackWriteMask(writeMask);
                })

                MAXWELL3D_CASE(windowOriginMode, {
                    context.SetViewportOrigin(windowOriginMode.isOriginLowerLeft);
                })

                MAXWELL3D_CASE(independentBlendEnable, {
                    context.SetIndependentBlendingEnabled(independentBlendEnable);
                })

                MAXWELL3D_CASE(alphaTestEnable, {
                    context.SetAlphaTestEnabled(alphaTestEnable);
                })

                #define SET_COLOR_BLEND_CONSTANT_CALLBACK(z, index, data) \
                MAXWELL3D_ARRAY_CASE(blendConstant, index, {              \
                    context.SetColorBlendConstant(index, blendConstant);  \
                })

                BOOST_PP_REPEAT(4, SET_COLOR_BLEND_CONSTANT_CALLBACK, 0)
                static_assert(type::BlendColorChannelCount == 4 && type::BlendColorChannelCount < BOOST_PP_LIMIT_REPEAT);
                #undef SET_COLOR_BLEND_CONSTANT_CALLBACK

                MAXWELL3D_STRUCT_CASE(blendStateCommon, colorOp, {
                    context.SetColorBlendOp(colorOp);
                })

                MAXWELL3D_STRUCT_CASE(blendStateCommon, colorSrcFactor, {
                    context.SetSrcColorBlendFactor(colorSrcFactor);
                })

                MAXWELL3D_STRUCT_CASE(blendStateCommon, colorDstFactor, {
                    context.SetDstColorBlendFactor(colorDstFactor);
                })

                MAXWELL3D_STRUCT_CASE(blendStateCommon, alphaOp, {
                    context.SetAlphaBlendOp(alphaOp);
                })

                MAXWELL3D_STRUCT_CASE(blendStateCommon, alphaSrcFactor, {
                    context.SetSrcAlphaBlendFactor(alphaSrcFactor);
                })

                MAXWELL3D_STRUCT_CASE(blendStateCommon, alphaDstFactor, {
                    context.SetDstAlphaBlendFactor(alphaDstFactor);
                })

                MAXWELL3D_STRUCT_CASE(blendStateCommon, enable, {
                    context.SetColorBlendEnabled(enable);
                })

                #define SET_COLOR_BLEND_ENABLE_CALLBACK(z, index, data) \
                MAXWELL3D_ARRAY_CASE(rtBlendEnable, index, {            \
                    context.SetColorBlendEnabled(index, rtBlendEnable); \
                })

                BOOST_PP_REPEAT(8, SET_COLOR_BLEND_ENABLE_CALLBACK, 0)
                static_assert(type::RenderTargetCount == 8 && type::RenderTargetCount < BOOST_PP_LIMIT_REPEAT);
                #undef SET_COLOR_BLEND_ENABLE_CALLBACK

                MAXWELL3D_CASE(lineWidthSmooth, {
                    if (*registers.lineSmoothEnable)
                        context.SetLineWidth(lineWidthSmooth);
                })

                MAXWELL3D_CASE(lineWidthAliased, {
                    if (!*registers.lineSmoothEnable)
                        context.SetLineWidth(lineWidthAliased);
                })

                MAXWELL3D_CASE(depthBiasFactor, {
                    context.SetDepthBiasSlopeFactor(depthBiasFactor);
                })

                MAXWELL3D_CASE(lineSmoothEnable, {
                    context.SetLineWidth(lineSmoothEnable ? *registers.lineWidthSmooth : *registers.lineWidthAliased);
                })

                MAXWELL3D_CASE(depthBiasUnits, {
                    context.SetDepthBiasConstantFactor(depthBiasUnits / 2.0f);
                })

                MAXWELL3D_STRUCT_CASE(setProgramRegion, high, {
                    context.SetShaderBaseIovaHigh(high);
                })

                MAXWELL3D_STRUCT_CASE(setProgramRegion, low, {
                    context.SetShaderBaseIovaLow(low);
                })

                MAXWELL3D_CASE(provokingVertexIsLast, {
                    context.SetProvokingVertex(provokingVertexIsLast);
                })

                MAXWELL3D_CASE(depthBiasClamp, {
                    context.SetDepthBiasClamp(depthBiasClamp);
                })

                MAXWELL3D_CASE(cullFaceEnable, {
                    context.SetCullFaceEnabled(cullFaceEnable);
                })

                MAXWELL3D_CASE(frontFace, {
                    context.SetFrontFace(frontFace);
                })

                MAXWELL3D_CASE(cullFace, {
                    context.SetCullFace(cullFace);
                })

                #define SET_COLOR_WRITE_MASK_CALLBACK(z, index, data)              \
                MAXWELL3D_ARRAY_CASE(colorWriteMask, index, {                      \
                    if (*registers.commonColorWriteMask)                           \
                        if (index == 0)                                            \
                            for (u32 idx{}; idx != type::RenderTargetCount; idx++) \
                                context.SetColorWriteMask(idx, colorWriteMask);    \
                    else                                                           \
                        context.SetColorWriteMask(index, colorWriteMask);          \
                })

                BOOST_PP_REPEAT(8, SET_COLOR_WRITE_MASK_CALLBACK, 2)
                static_assert(type::RenderTargetCount == 8 && type::RenderTargetCount < BOOST_PP_LIMIT_REPEAT);
                #undef SET_COLOR_WRITE_MASK_CALLBACK

                MAXWELL3D_CASE(viewVolumeClipControl, {
                    context.SetDepthClampEnabled(!viewVolumeClipControl.depthClampDisable);
                })

                MAXWELL3D_STRUCT_CASE(colorLogicOp, enable, {
                    context.SetBlendLogicOpEnable(enable);
                })

                MAXWELL3D_STRUCT_CASE(colorLogicOp, type, {
                    context.SetBlendLogicOpType(type);
                })

                #define VERTEX_BUFFER_CALLBACKS(z, index, data)                            \
                MAXWELL3D_ARRAY_STRUCT_CASE(vertexBuffers, index, config, {                \
                    context.SetVertexBufferStride(index, config.stride);                   \
                })                                                                         \
                MAXWELL3D_ARRAY_STRUCT_STRUCT_CASE(vertexBuffers, index, iova, high, {     \
                    context.SetVertexBufferStartIovaHigh(index, high);                     \
                })                                                                         \
                MAXWELL3D_ARRAY_STRUCT_STRUCT_CASE(vertexBuffers, index, iova, low, {      \
                    context.SetVertexBufferStartIovaLow(index, low);                       \
                })                                                                         \
                MAXWELL3D_ARRAY_STRUCT_CASE(vertexBuffers, index, divisor, {               \
                    context.SetVertexBufferDivisor(index, divisor);                        \
                })                                                                         \
                MAXWELL3D_ARRAY_CASE(isVertexInputRatePerInstance, index, {                \
                    context.SetVertexBufferInputRate(index, isVertexInputRatePerInstance); \
                })                                                                         \
                MAXWELL3D_ARRAY_STRUCT_CASE(vertexBufferLimits, index, high, {             \
                    context.SetVertexBufferEndIovaHigh(index, high);                       \
                })                                                                         \
                MAXWELL3D_ARRAY_STRUCT_CASE(vertexBufferLimits, index, low, {              \
                    context.SetVertexBufferEndIovaLow(index, low);                         \
                })

                BOOST_PP_REPEAT(16, VERTEX_BUFFER_CALLBACKS, 0)
                static_assert(type::VertexBufferCount == 16 && type::VertexBufferCount < BOOST_PP_LIMIT_REPEAT);
                #undef VERTEX_BUFFER_CALLBACKS

                #define VERTEX_ATTRIBUTES_CALLBACKS(z, index, data)               \
                MAXWELL3D_ARRAY_CASE(vertexAttributeState, index, {               \
                    context.SetVertexAttributeState(index, vertexAttributeState); \
                })

                BOOST_PP_REPEAT(32, VERTEX_ATTRIBUTES_CALLBACKS, 0)
                static_assert(type::VertexAttributeCount == 32 && type::VertexAttributeCount < BOOST_PP_LIMIT_REPEAT);
                #undef VERTEX_BUFFER_CALLBACKS

                #define SET_INDEPENDENT_COLOR_BLEND_CALLBACKS(z, index, data)          \
                MAXWELL3D_ARRAY_STRUCT_CASE(independentBlend, index, colorOp, {        \
                    context.SetColorBlendOp(index, colorOp);                           \
                })                                                                     \
                MAXWELL3D_ARRAY_STRUCT_CASE(independentBlend, index, colorSrcFactor, { \
                    context.SetSrcColorBlendFactor(index, colorSrcFactor);             \
                })                                                                     \
                MAXWELL3D_ARRAY_STRUCT_CASE(independentBlend, index, colorDstFactor, { \
                    context.SetDstColorBlendFactor(index, colorDstFactor);             \
                })                                                                     \
                MAXWELL3D_ARRAY_STRUCT_CASE(independentBlend, index, alphaOp, {        \
                    context.SetAlphaBlendOp(index, alphaOp);                           \
                })                                                                     \
                MAXWELL3D_ARRAY_STRUCT_CASE(independentBlend, index, alphaSrcFactor, { \
                    context.SetSrcAlphaBlendFactor(index, alphaSrcFactor);             \
                })                                                                     \
                MAXWELL3D_ARRAY_STRUCT_CASE(independentBlend, index, alphaDstFactor, { \
                    context.SetDstAlphaBlendFactor(index, alphaDstFactor);             \
                })

                BOOST_PP_REPEAT(8, SET_INDEPENDENT_COLOR_BLEND_CALLBACKS, 0)
                static_assert(type::RenderTargetCount == 8 && type::RenderTargetCount < BOOST_PP_LIMIT_REPEAT);
                #undef SET_COLOR_BLEND_ENABLE_CALLBACK

                #define SET_SHADER_ENABLE_CALLBACK(z, index, data)     \
                MAXWELL3D_ARRAY_STRUCT_CASE(setProgram, index, info, { \
                    context.SetShaderEnabled(info.stage, info.enable); \
                })

                BOOST_PP_REPEAT(6, SET_SHADER_ENABLE_CALLBACK, 0)
                static_assert(type::ShaderStageCount == 6 && type::ShaderStageCount < BOOST_PP_LIMIT_REPEAT);
                #undef SET_SHADER_ENABLE_CALLBACK

                MAXWELL3D_CASE(vertexBeginGl, {
                    context.SetPrimitiveTopology(vertexBeginGl.topology);
                })

                MAXWELL3D_STRUCT_CASE(constantBufferSelector, size, {
                    context.SetConstantBufferSelectorSize(size);
                })

                MAXWELL3D_STRUCT_STRUCT_CASE(constantBufferSelector, address, high, {
                    context.SetConstantBufferSelectorIovaHigh(high);
                })

                MAXWELL3D_STRUCT_STRUCT_CASE(constantBufferSelector, address, low, {
                    context.SetConstantBufferSelectorIovaLow(low);
                })

                MAXWELL3D_STRUCT_STRUCT_CASE(indexBuffer, start, high, {
                    context.SetIndexBufferStartIovaHigh(high);
                })
                MAXWELL3D_STRUCT_STRUCT_CASE(indexBuffer, start, low, {
                    context.SetIndexBufferStartIovaLow(low);
                })
                MAXWELL3D_STRUCT_STRUCT_CASE(indexBuffer, limit, high, {
                    context.SetIndexBufferEndIovaHigh(high);
                })
                MAXWELL3D_STRUCT_STRUCT_CASE(indexBuffer, limit, low, {
                    context.SetIndexBufferEndIovaLow(low);
                })
                MAXWELL3D_STRUCT_CASE(indexBuffer, format, {
                    context.SetIndexBufferFormat(format);
                })

                MAXWELL3D_CASE(bindlessTextureConstantBufferIndex, {
                    context.SetBindlessTextureConstantBufferIndex(bindlessTextureConstantBufferIndex);
                })

                MAXWELL3D_STRUCT_STRUCT_CASE(samplerPool, address, high, {
                    context.SetSamplerPoolIovaHigh(high);
                })
                MAXWELL3D_STRUCT_STRUCT_CASE(samplerPool, address, low, {
                    context.SetSamplerPoolIovaLow(low);
                })
                MAXWELL3D_STRUCT_CASE(samplerPool, maximumIndex, {
                    context.SetSamplerPoolMaximumIndex(maximumIndex);
                })

                MAXWELL3D_STRUCT_STRUCT_CASE(texturePool, address, high, {
                    context.SetTexturePoolIovaHigh(high);
                })
                MAXWELL3D_STRUCT_STRUCT_CASE(texturePool, address, low, {
                    context.SetTexturePoolIovaLow(low);
                })
                MAXWELL3D_STRUCT_CASE(texturePool, maximumIndex, {
                    context.SetTexturePoolMaximumIndex(maximumIndex);
                })

                MAXWELL3D_STRUCT_CASE(constantBufferUpdate, offset, {
                    context.SetConstantBufferUpdateOffset(offset);
                })

                default:
                    break;
            }
        }

        switch (method) {
            MAXWELL3D_STRUCT_CASE(mme, instructionRamLoad, {
                if (registers.mme->instructionRamPointer >= macroState.macroCode.size())
                    throw exception("Macro memory is full!");

                macroState.macroCode[registers.mme->instructionRamPointer++] = instructionRamLoad;

                // Wraparound writes
                // This works on HW but will also generate an error interrupt
                registers.mme->instructionRamPointer %= macroState.macroCode.size();
            })

            MAXWELL3D_STRUCT_CASE(mme, startAddressRamLoad, {
                if (registers.mme->startAddressRamPointer >= macroState.macroPositions.size())
                    throw exception("Maximum amount of macros reached!");

                macroState.macroPositions[registers.mme->startAddressRamPointer++] = startAddressRamLoad;
            })

            MAXWELL3D_CASE(syncpointAction, {
                Logger::Debug("Increment syncpoint: {}", static_cast<u16>(syncpointAction.id));
                channelCtx.executor.Execute();
                syncpoints.at(syncpointAction.id).Increment();
            })

            MAXWELL3D_CASE(clearBuffers, {
                context.ClearBuffers(clearBuffers);
            })

            MAXWELL3D_CASE(drawVertexCount, {
                context.DrawVertex(drawVertexCount, *registers.drawVertexFirst);
            })

            MAXWELL3D_CASE(drawIndexCount, {
                context.DrawIndexed(drawIndexCount, *registers.drawIndexFirst, *registers.drawBaseVertex);
            })

            MAXWELL3D_STRUCT_CASE(semaphore, info, {
                switch (info.op) {
                    case type::SemaphoreInfo::Op::Release:
                        WriteSemaphoreResult(registers.semaphore->payload);
                        break;

                    case type::SemaphoreInfo::Op::Counter: {
                        switch (info.counterType) {
                            case type::SemaphoreInfo::CounterType::Zero:
                                WriteSemaphoreResult(0);
                                break;

                            default:
                                Logger::Warn("Unsupported semaphore counter type: 0x{:X}", static_cast<u8>(info.counterType));
                                break;
                        }
                        break;
                    }

                    default:
                        Logger::Warn("Unsupported semaphore operation: 0x{:X}", static_cast<u8>(info.op));
                        break;
                }
            })

            #define SHADER_CALLBACKS(z, index, data)                                        \
                MAXWELL3D_ARRAY_STRUCT_CASE(setProgram, index, offset, {                    \
                    context.SetShaderOffset(static_cast<type::ShaderStage>(index), offset); \
                })

            BOOST_PP_REPEAT(6, SHADER_CALLBACKS, 0)
            static_assert(type::ShaderStageCount == 6 && type::ShaderStageCount < BOOST_PP_LIMIT_REPEAT);
            #undef SHADER_CALLBACKS

            #define PIPELINE_CALLBACKS(z, idx, data)                                                                                       \
                MAXWELL3D_ARRAY_STRUCT_CASE(bind, idx, constantBuffer, {                                                                   \
                    context.BindPipelineConstantBuffer(static_cast<type::PipelineStage>(idx), constantBuffer.valid, constantBuffer.index); \
                })

            BOOST_PP_REPEAT(5, PIPELINE_CALLBACKS, 0)
            static_assert(type::PipelineStageCount == 5 && type::PipelineStageCount < BOOST_PP_LIMIT_REPEAT);
            #undef PIPELINE_CALLBACKS

            MAXWELL3D_ARRAY_CASE(firmwareCall, 4, {
                registers.raw[0xD00] = 1;
            })

            #define CBUF_UPDATE_CALLBACKS(z, index, data_)                   \
            MAXWELL3D_STRUCT_ARRAY_CASE(constantBufferUpdate, data, index, { \
                context.ConstantBufferUpdate(data);                          \
            })

            BOOST_PP_REPEAT(16, CBUF_UPDATE_CALLBACKS, 0)
            #undef CBUF_UPDATE_CALLBACKS

            default:
                break;
        }

        #undef MAXWELL3D_OFFSET
        #undef MAXWELL3D_STRUCT_OFFSET
        #undef MAXWELL3D_STRUCT_ARRAY_OFFSET
        #undef MAXWELL3D_ARRAY_OFFSET
        #undef MAXWELL3D_ARRAY_STRUCT_OFFSET
        #undef MAXWELL3D_ARRAY_STRUCT_STRUCT_OFFSET

        #undef MAXWELL3D_CASE_BASE
        #undef MAXWELL3D_CASE
        #undef MAXWELL3D_STRUCT_CASE
        #undef MAXWELL3D_STRUCT_ARRAY_CASE
        #undef MAXWELL3D_ARRAY_CASE
        #undef MAXWELL3D_ARRAY_STRUCT_CASE
        #undef MAXWELL3D_ARRAY_STRUCT_STRUCT_CASE
    }

    void Maxwell3D::WriteSemaphoreResult(u64 result) {
        struct FourWordResult {
            u64 value;
            u64 timestamp;
        };

        switch (registers.semaphore->info.structureSize) {
            case type::SemaphoreInfo::StructureSize::OneWord:
                channelCtx.asCtx->gmmu.Write<u32>(registers.semaphore->address, static_cast<u32>(result));
                break;

            case type::SemaphoreInfo::StructureSize::FourWords: {
                // Convert the current nanosecond time to GPU ticks
                constexpr i64 NsToTickNumerator{384};
                constexpr i64 NsToTickDenominator{625};

                i64 nsTime{util::GetTimeNs()};
                i64 timestamp{(nsTime / NsToTickDenominator) * NsToTickNumerator + ((nsTime % NsToTickDenominator) * NsToTickNumerator) / NsToTickDenominator};

                channelCtx.asCtx->gmmu.Write<FourWordResult>(registers.semaphore->address,
                                                             FourWordResult{result, static_cast<u64>(timestamp)});
                break;
            }
        }
    }
}
