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
          i2m(channelCtx),
          channelCtx(channelCtx) {
        executor.AddFlushCallback([this]() { FlushEngineState(); });
        InitializeRegisters();
    }

    void Maxwell3D::FlushDeferredDraw() {
        if (deferredDraw.pending) {
            deferredDraw.pending = false;

            if (deferredDraw.indexed)
                context.DrawIndexed(deferredDraw.drawCount, deferredDraw.drawFirst, deferredDraw.instanceCount, deferredDraw.drawBaseVertex);
            else
                context.Draw(deferredDraw.drawCount, deferredDraw.drawFirst, deferredDraw.instanceCount);

            deferredDraw.instanceCount = 1;
        }
    }

    void Maxwell3D::HandleMethod(u32 method, u32 argument) {
        if (method != ENGINE_STRUCT_OFFSET(mme, shadowRamControl)) {
            if (shadowRegisters.mme->shadowRamControl == type::MmeShadowRamControl::MethodTrack || shadowRegisters.mme->shadowRamControl == type::MmeShadowRamControl::MethodTrackWithFilter)
                shadowRegisters.raw[method] = argument;
            else if (shadowRegisters.mme->shadowRamControl == type::MmeShadowRamControl::MethodReplay)
                argument = shadowRegisters.raw[method];
        }

        bool redundant{registers.raw[method] == argument};
        registers.raw[method] = argument;

        if (batchConstantBufferUpdate.Active()) {
            switch (method) {
                // Add to the batch constant buffer update buffer
                // Return early here so that any code below can rely on the fact that any cbuf updates will always be the first of a batch
                #define CBUF_UPDATE_CALLBACKS(z, index, data_)                \
                ENGINE_STRUCT_ARRAY_CASE(constantBufferUpdate, data, index, { \
                    batchConstantBufferUpdate.buffer.push_back(data);         \
                    registers.constantBufferUpdate->offset += 4;              \
                    return;                                                   \
                })

                BOOST_PP_REPEAT(16, CBUF_UPDATE_CALLBACKS, 0)
                #undef CBUF_UPDATE_CALLBACKS
                default:
                    // When a method other than constant buffer update is called submit our submit the previously built-up update as a batch
                    context.ConstantBufferUpdate(batchConstantBufferUpdate.buffer, batchConstantBufferUpdate.Invalidate());
                    batchConstantBufferUpdate.Reset();
                    break; // Continue on here to handle the actual method
            }
        }

        // See DeferredDrawState comment for full details
        if (deferredDraw.pending) {
            switch (method) {
                ENGINE_CASE(vertexBeginGl, {
                    if (vertexBeginGl.instanceNext) {
                        if (deferredDraw.drawTopology != vertexBeginGl.topology && !vertexBeginGl.instanceContinue)
                            Logger::Warn("Vertex topology changed partway through instanced draw!");

                        deferredDraw.instanceCount++;
                    } else if (vertexBeginGl.instanceContinue) {
                        FlushDeferredDraw();
                        break; // This instanced draw is finished, continue on to handle the actual method
                    }

                    return;
                })

                // Can be ignored since we handle drawing in draw{Vertex,Index}Count
                ENGINE_CASE(vertexEndGl, { return; })

                // Draws here can be ignored since they're just repeats of the original instanced draw
                ENGINE_CASE(drawVertexCount, {
                    if (!redundant)
                        Logger::Warn("Vertex count changed partway through instanced draw!");
                    return;
                })
                ENGINE_CASE(drawIndexCount, {
                    if (!redundant)
                        Logger::Warn("Index count changed partway through instanced draw!");
                    return;
                })

                // Once we stop calling draw methods flush the current draw since drawing is dependent on the register state not changing
                default:
                    FlushDeferredDraw();
                    break;
            }
        }

        if (!redundant) {
            switch (method) {
                ENGINE_STRUCT_CASE(mme, shadowRamControl, {
                    shadowRegisters.mme->shadowRamControl = shadowRamControl;
                })

                #define RENDER_TARGET_ARRAY(z, index, data)                               \
                ENGINE_ARRAY_STRUCT_STRUCT_CASE(renderTargets, index, address, high, {    \
                    context.SetColorRenderTargetAddressHigh(index, high);                 \
                })                                                                        \
                ENGINE_ARRAY_STRUCT_STRUCT_CASE(renderTargets, index, address, low, {     \
                    context.SetColorRenderTargetAddressLow(index, low);                   \
                })                                                                        \
                ENGINE_ARRAY_STRUCT_CASE(renderTargets, index, width, {                   \
                    context.SetColorRenderTargetWidth(index, width);                      \
                })                                                                        \
                ENGINE_ARRAY_STRUCT_CASE(renderTargets, index, height, {                  \
                    context.SetColorRenderTargetHeight(index, height);                    \
                })                                                                        \
                ENGINE_ARRAY_STRUCT_CASE(renderTargets, index, format, {                  \
                    context.SetColorRenderTargetFormat(index, format);                    \
                })                                                                        \
                ENGINE_ARRAY_STRUCT_CASE(renderTargets, index, tileMode, {                \
                    context.SetColorRenderTargetTileMode(index, tileMode);                \
                })                                                                        \
                ENGINE_ARRAY_STRUCT_CASE(renderTargets, index, arrayMode, {               \
                    context.SetColorRenderTargetArrayMode(index, arrayMode);              \
                })                                                                        \
                ENGINE_ARRAY_STRUCT_CASE(renderTargets, index, layerStrideLsr2, {         \
                    context.SetColorRenderTargetLayerStride(index, layerStrideLsr2);      \
                })                                                                        \
                ENGINE_ARRAY_STRUCT_CASE(renderTargets, index, baseLayer, {               \
                    context.SetColorRenderTargetBaseLayer(index, baseLayer);              \
                })

                BOOST_PP_REPEAT(8, RENDER_TARGET_ARRAY, 0)
                static_assert(type::RenderTargetCount == 8 && type::RenderTargetCount < BOOST_PP_LIMIT_REPEAT);
                #undef RENDER_TARGET_ARRAY

                ENGINE_CASE(pointSpriteSize, {
                    context.SetPointSpriteSize(pointSpriteSize);
                })

                ENGINE_CASE(depthTargetEnable, {
                    context.SetDepthRenderTargetEnabled(depthTargetEnable);
                })
                ENGINE_STRUCT_CASE(depthTargetAddress, high, {
                    context.SetDepthRenderTargetAddressHigh(high);
                })
                ENGINE_STRUCT_CASE(depthTargetAddress, low, {
                    context.SetDepthRenderTargetAddressLow(low);
                })
                ENGINE_CASE(depthTargetFormat, {
                    context.SetDepthRenderTargetFormat(depthTargetFormat);
                })
                ENGINE_CASE(depthTargetTileMode, {
                    context.SetDepthRenderTargetTileMode(depthTargetTileMode);
                })
                ENGINE_CASE(depthTargetLayerStride, {
                    context.SetDepthRenderTargetLayerStride(depthTargetLayerStride);
                })
                ENGINE_CASE(depthTargetWidth, {
                    context.SetDepthRenderTargetWidth(depthTargetWidth);
                })
                ENGINE_CASE(depthTargetHeight, {
                    context.SetDepthRenderTargetHeight(depthTargetHeight);
                })
                ENGINE_CASE(depthTargetArrayMode, {
                    context.SetDepthRenderTargetArrayMode(depthTargetArrayMode);
                })

                ENGINE_CASE(linkedTscHandle, {
                    context.SetTscIndexLinked(linkedTscHandle);
                });

                #define VIEWPORT_TRANSFORM_CALLBACKS(_z, index, data)                                     \
                ENGINE_ARRAY_STRUCT_CASE(viewportTransforms, index, scaleX, {                             \
                    context.SetViewportX(index, scaleX, registers.viewportTransforms[index].translateX);  \
                })                                                                                        \
                ENGINE_ARRAY_STRUCT_CASE(viewportTransforms, index, translateX, {                         \
                    context.SetViewportX(index, registers.viewportTransforms[index].scaleX, translateX);  \
                })                                                                                        \
                ENGINE_ARRAY_STRUCT_CASE(viewportTransforms, index, scaleY, {                             \
                    context.SetViewportY(index, scaleY, registers.viewportTransforms[index].translateY);  \
                })                                                                                        \
                ENGINE_ARRAY_STRUCT_CASE(viewportTransforms, index, translateY, {                         \
                    context.SetViewportY(index, registers.viewportTransforms[index].scaleY, translateY);  \
                })                                                                                        \
                ENGINE_ARRAY_STRUCT_CASE(viewportTransforms, index, scaleZ, {                             \
                    context.SetViewportZ(index, scaleZ, registers.viewportTransforms[index].translateZ);  \
                })                                                                                        \
                ENGINE_ARRAY_STRUCT_CASE(viewportTransforms, index, translateZ, {                         \
                    context.SetViewportZ(index, registers.viewportTransforms[index].scaleZ, translateZ);  \
                })                                                                                        \
                ENGINE_ARRAY_STRUCT_CASE(viewportTransforms, index, swizzles, {                           \
                    context.SetViewportSwizzle(index, swizzles.x, swizzles.y, swizzles.z, swizzles.w);    \
                })

                BOOST_PP_REPEAT(16, VIEWPORT_TRANSFORM_CALLBACKS, 0)
                static_assert(type::ViewportCount == 16 && type::ViewportCount < BOOST_PP_LIMIT_REPEAT);
                #undef VIEWPORT_TRANSFORM_CALLBACKS

                #define COLOR_CLEAR_CALLBACKS(z, index, data)              \
                ENGINE_ARRAY_CASE(clearColorValue, index, {                \
                    context.UpdateClearColorValue(index, clearColorValue); \
                })

                BOOST_PP_REPEAT(4, COLOR_CLEAR_CALLBACKS, 0)
                static_assert(4 < BOOST_PP_LIMIT_REPEAT);
                #undef COLOR_CLEAR_CALLBACKS

                ENGINE_CASE(clearDepthValue, {
                    context.UpdateClearDepthValue(clearDepthValue);
                })

                ENGINE_CASE(clearStencilValue, {
                    context.UpdateClearStencilValue(clearStencilValue);
                })

                ENGINE_STRUCT_CASE(polygonMode, front, {
                    context.SetPolygonModeFront(front);
                })

                ENGINE_CASE(tessellationPatchSize, {
                    context.SetTessellationPatchSize(tessellationPatchSize);
                })

                ENGINE_CASE(tessellationMode, {
                    context.SetTessellationMode(tessellationMode.primitive, tessellationMode.spacing, tessellationMode.winding);
                })

                #define TRANSFORM_FEEDBACK_CALLBACKS(z, index, data)                           \
                ENGINE_ARRAY_STRUCT_CASE(transformFeedbackBuffers, index, enable, {            \
                    context.SetTransformFeedbackBufferEnabled(index, enable);                  \
                })                                                                             \
                ENGINE_ARRAY_STRUCT_STRUCT_CASE(transformFeedbackBuffers, index, iova, high, { \
                    context.SetTransformFeedbackBufferIovaHigh(index, high);                   \
                })                                                                             \
                ENGINE_ARRAY_STRUCT_STRUCT_CASE(transformFeedbackBuffers, index, iova, low, {  \
                    context.SetTransformFeedbackBufferIovaLow(index, low);                     \
                })                                                                             \
                ENGINE_ARRAY_STRUCT_CASE(transformFeedbackBuffers, index, size, {              \
                    context.SetTransformFeedbackBufferSize(index, size);                       \
                })                                                                             \
                ENGINE_ARRAY_STRUCT_CASE(transformFeedbackBuffers, index, offset, {            \
                    context.SetTransformFeedbackBufferOffset(index, offset);                   \
                })                                                                             \
                ENGINE_ARRAY_STRUCT_CASE(transformFeedbackBufferStates, index, varyingCount, { \
                    context.SetTransformFeedbackBufferVaryingCount(index, varyingCount);       \
                })                                                                             \
                ENGINE_ARRAY_STRUCT_CASE(transformFeedbackBufferStates, index, stride, {       \
                    context.SetTransformFeedbackBufferStride(index, stride);                   \
                })

                BOOST_PP_REPEAT(4, TRANSFORM_FEEDBACK_CALLBACKS, 0)
                static_assert(type::TransformFeedbackBufferCount == 4 && type::TransformFeedbackBufferCount < BOOST_PP_LIMIT_REPEAT);
                #undef TRANSFORM_FEEDBACK_CALLBACKS

                ENGINE_CASE(transformFeedbackEnable, {
                    context.SetTransformFeedbackEnabled(transformFeedbackEnable);
                })

                ENGINE_STRUCT_CASE(depthBiasEnable, point, {
                    context.SetDepthBiasPointEnabled(point);
                })

                ENGINE_STRUCT_CASE(depthBiasEnable, line, {
                    context.SetDepthBiasLineEnabled(line);
                })

                ENGINE_STRUCT_CASE(depthBiasEnable, fill, {
                    context.SetDepthBiasFillEnabled(fill);
                })

                #define SCISSOR_CALLBACKS(z, index, data)                                                           \
                ENGINE_ARRAY_STRUCT_CASE(scissors, index, enable, {                                                 \
                    context.SetScissor(index, enable ? registers.scissors[index] : std::optional<type::Scissor>{}); \
                })                                                                                                  \
                ENGINE_ARRAY_STRUCT_CASE(scissors, index, horizontal, {                                             \
                    context.SetScissorHorizontal(index, horizontal);                                                \
                })                                                                                                  \
                ENGINE_ARRAY_STRUCT_CASE(scissors, index, vertical, {                                               \
                    context.SetScissorVertical(index, vertical);                                                    \
                })

                BOOST_PP_REPEAT(16, SCISSOR_CALLBACKS, 0)
                static_assert(type::ViewportCount == 16 && type::ViewportCount < BOOST_PP_LIMIT_REPEAT);
                #undef SCISSOR_CALLBACKS

                ENGINE_CASE(commonColorWriteMask, {
                    if (commonColorWriteMask) {
                        auto colorWriteMask{registers.colorWriteMask[0]};
                        for (u32 index{}; index != type::RenderTargetCount; index++)
                            context.SetColorWriteMask(index, colorWriteMask);
                    } else {
                        for (u32 index{}; index != type::RenderTargetCount; index++)
                            context.SetColorWriteMask(index, registers.colorWriteMask[index]);
                    }
                })

                ENGINE_CASE(renderTargetControl, {
                    context.UpdateRenderTargetControl(renderTargetControl);
                })

                ENGINE_CASE(depthTestEnable, {
                    context.SetDepthTestEnabled(depthTestEnable);
                })

                ENGINE_CASE(depthTestFunc, {
                    context.SetDepthTestFunction(depthTestFunc);
                })

                ENGINE_CASE(depthWriteEnable, {
                    context.SetDepthWriteEnabled(depthWriteEnable);
                })

                ENGINE_CASE(depthBoundsEnable, {
                    context.SetDepthBoundsTestEnabled(depthBoundsEnable);
                })

                ENGINE_CASE(depthBoundsNear, {
                    context.SetMinDepthBounds(depthBoundsNear);
                })

                ENGINE_CASE(depthBoundsFar, {
                    context.SetMaxDepthBounds(depthBoundsFar);
                })

                ENGINE_CASE(stencilEnable, {
                    context.SetStencilTestEnabled(stencilEnable);
                })

                ENGINE_STRUCT_CASE(stencilFront, failOp, {
                    context.SetStencilFrontFailOp(failOp);
                })

                ENGINE_STRUCT_CASE(stencilFront, zFailOp, {
                    context.SetStencilFrontDepthFailOp(zFailOp);
                })

                ENGINE_STRUCT_CASE(stencilFront, passOp, {
                    context.SetStencilFrontPassOp(passOp);
                })

                ENGINE_STRUCT_CASE(stencilFront, compareOp, {
                    context.SetStencilFrontCompareOp(compareOp);
                })

                ENGINE_STRUCT_CASE(stencilFront, compareReference, {
                    context.SetStencilFrontReference(compareReference);
                })

                ENGINE_STRUCT_CASE(stencilFront, compareMask, {
                    context.SetStencilFrontCompareMask(compareMask);
                })

                ENGINE_STRUCT_CASE(stencilFront, writeMask, {
                    context.SetStencilFrontWriteMask(writeMask);
                })

                ENGINE_CASE(stencilTwoSideEnable, {
                    context.SetStencilTwoSideEnabled(stencilTwoSideEnable);
                })

                ENGINE_STRUCT_CASE(stencilBack, failOp, {
                    context.SetStencilBackFailOp(failOp);
                })

                ENGINE_STRUCT_CASE(stencilBack, zFailOp, {
                    context.SetStencilBackDepthFailOp(zFailOp);
                })

                ENGINE_STRUCT_CASE(stencilBack, passOp, {
                    context.SetStencilBackPassOp(passOp);
                })

                ENGINE_STRUCT_CASE(stencilBack, compareOp, {
                    context.SetStencilBackCompareOp(compareOp);
                })

                ENGINE_STRUCT_CASE(stencilBackExtra, compareReference, {
                    context.SetStencilBackReference(compareReference);
                })

                ENGINE_STRUCT_CASE(stencilBackExtra, compareMask, {
                    context.SetStencilBackCompareMask(compareMask);
                })

                ENGINE_STRUCT_CASE(stencilBackExtra, writeMask, {
                    context.SetStencilBackWriteMask(writeMask);
                })

                ENGINE_CASE(windowOriginMode, {
                    context.SetViewportOrigin(windowOriginMode.isOriginLowerLeft);
                    context.SetFrontFaceFlipEnabled(windowOriginMode.flipFrontFace);
                })

                ENGINE_CASE(independentBlendEnable, {
                    context.SetIndependentBlendingEnabled(independentBlendEnable);
                })

                ENGINE_CASE(alphaTestEnable, {
                    context.SetAlphaTestEnabled(alphaTestEnable);
                })

                #define SET_COLOR_BLEND_CONSTANT_CALLBACK(z, index, data) \
                ENGINE_ARRAY_CASE(blendConstant, index, {                 \
                    context.SetColorBlendConstant(index, blendConstant);  \
                })

                BOOST_PP_REPEAT(4, SET_COLOR_BLEND_CONSTANT_CALLBACK, 0)
                static_assert(type::BlendColorChannelCount == 4 && type::BlendColorChannelCount < BOOST_PP_LIMIT_REPEAT);
                #undef SET_COLOR_BLEND_CONSTANT_CALLBACK

                ENGINE_STRUCT_CASE(blendStateCommon, colorOp, {
                    context.SetColorBlendOp(colorOp);
                })

                ENGINE_STRUCT_CASE(blendStateCommon, colorSrcFactor, {
                    context.SetSrcColorBlendFactor(colorSrcFactor);
                })

                ENGINE_STRUCT_CASE(blendStateCommon, colorDstFactor, {
                    context.SetDstColorBlendFactor(colorDstFactor);
                })

                ENGINE_STRUCT_CASE(blendStateCommon, alphaOp, {
                    context.SetAlphaBlendOp(alphaOp);
                })

                ENGINE_STRUCT_CASE(blendStateCommon, alphaSrcFactor, {
                    context.SetSrcAlphaBlendFactor(alphaSrcFactor);
                })

                ENGINE_STRUCT_CASE(blendStateCommon, alphaDstFactor, {
                    context.SetDstAlphaBlendFactor(alphaDstFactor);
                })

                #define SET_COLOR_BLEND_ENABLE_CALLBACK(z, index, data) \
                ENGINE_ARRAY_CASE(rtBlendEnable, index, {               \
                    context.SetColorBlendEnabled(index, rtBlendEnable); \
                })

                BOOST_PP_REPEAT(8, SET_COLOR_BLEND_ENABLE_CALLBACK, 0)
                static_assert(type::RenderTargetCount == 8 && type::RenderTargetCount < BOOST_PP_LIMIT_REPEAT);
                #undef SET_COLOR_BLEND_ENABLE_CALLBACK

                ENGINE_CASE(lineWidthSmooth, {
                    if (*registers.lineSmoothEnable)
                        context.SetLineWidth(lineWidthSmooth);
                })

                ENGINE_CASE(lineWidthAliased, {
                    if (!*registers.lineSmoothEnable)
                        context.SetLineWidth(lineWidthAliased);
                })

                ENGINE_CASE(depthBiasFactor, {
                    context.SetDepthBiasSlopeFactor(depthBiasFactor);
                })

                ENGINE_CASE(lineSmoothEnable, {
                    context.SetLineWidth(lineSmoothEnable ? *registers.lineWidthSmooth : *registers.lineWidthAliased);
                })

                ENGINE_CASE(depthBiasUnits, {
                    context.SetDepthBiasConstantFactor(depthBiasUnits / 2.0f);
                })

                ENGINE_STRUCT_CASE(setProgramRegion, high, {
                    context.SetShaderBaseIovaHigh(high);
                })

                ENGINE_STRUCT_CASE(setProgramRegion, low, {
                    context.SetShaderBaseIovaLow(low);
                })

                ENGINE_CASE(provokingVertexIsLast, {
                    context.SetProvokingVertex(provokingVertexIsLast);
                })

                ENGINE_CASE(depthBiasClamp, {
                    context.SetDepthBiasClamp(depthBiasClamp);
                })

                ENGINE_CASE(cullFaceEnable, {
                    context.SetCullFaceEnabled(cullFaceEnable);
                })

                ENGINE_CASE(frontFace, {
                    context.SetFrontFace(frontFace);
                })

                ENGINE_CASE(cullFace, {
                    context.SetCullFace(cullFace);
                })

                #define SET_COLOR_WRITE_MASK_CALLBACK(z, index, data)              \
                ENGINE_ARRAY_CASE(colorWriteMask, index, {                         \
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

                ENGINE_CASE(viewVolumeClipControl, {
                    context.SetDepthClampEnabled(!viewVolumeClipControl.depthClampDisable);
                })

                ENGINE_STRUCT_CASE(colorLogicOp, enable, {
                    context.SetBlendLogicOpEnable(enable);
                })

                ENGINE_STRUCT_CASE(colorLogicOp, type, {
                    context.SetBlendLogicOpType(type);
                })

                #define VERTEX_BUFFER_CALLBACKS(z, index, data)                            \
                ENGINE_ARRAY_STRUCT_CASE(vertexBuffers, index, config, {                   \
                    context.SetVertexBufferStride(index, config.stride);                   \
                })                                                                         \
                ENGINE_ARRAY_STRUCT_STRUCT_CASE(vertexBuffers, index, iova, high, {        \
                    context.SetVertexBufferStartIovaHigh(index, high);                     \
                })                                                                         \
                ENGINE_ARRAY_STRUCT_STRUCT_CASE(vertexBuffers, index, iova, low, {         \
                    context.SetVertexBufferStartIovaLow(index, low);                       \
                })                                                                         \
                ENGINE_ARRAY_STRUCT_CASE(vertexBuffers, index, divisor, {                  \
                    context.SetVertexBufferDivisor(index, divisor);                        \
                })                                                                         \
                ENGINE_ARRAY_CASE(isVertexInputRatePerInstance, index, {                   \
                    context.SetVertexBufferInputRate(index, isVertexInputRatePerInstance); \
                })                                                                         \
                ENGINE_ARRAY_STRUCT_CASE(vertexBufferLimits, index, high, {                \
                    context.SetVertexBufferEndIovaHigh(index, high);                       \
                })                                                                         \
                ENGINE_ARRAY_STRUCT_CASE(vertexBufferLimits, index, low, {                 \
                    context.SetVertexBufferEndIovaLow(index, low);                         \
                })

                BOOST_PP_REPEAT(16, VERTEX_BUFFER_CALLBACKS, 0)
                static_assert(type::VertexBufferCount == 16 && type::VertexBufferCount < BOOST_PP_LIMIT_REPEAT);
                #undef VERTEX_BUFFER_CALLBACKS

                #define VERTEX_ATTRIBUTES_CALLBACKS(z, index, data)               \
                ENGINE_ARRAY_CASE(vertexAttributeState, index, {                  \
                    context.SetVertexAttributeState(index, vertexAttributeState); \
                })

                BOOST_PP_REPEAT(32, VERTEX_ATTRIBUTES_CALLBACKS, 0)
                static_assert(type::VertexAttributeCount == 32 && type::VertexAttributeCount < BOOST_PP_LIMIT_REPEAT);
                #undef VERTEX_BUFFER_CALLBACKS

                #define SET_INDEPENDENT_COLOR_BLEND_CALLBACKS(z, index, data)          \
                ENGINE_ARRAY_STRUCT_CASE(independentBlend, index, colorOp, {           \
                    context.SetColorBlendOp(index, colorOp);                           \
                })                                                                     \
                ENGINE_ARRAY_STRUCT_CASE(independentBlend, index, colorSrcFactor, {    \
                    context.SetSrcColorBlendFactor(index, colorSrcFactor);             \
                })                                                                     \
                ENGINE_ARRAY_STRUCT_CASE(independentBlend, index, colorDstFactor, {    \
                    context.SetDstColorBlendFactor(index, colorDstFactor);             \
                })                                                                     \
                ENGINE_ARRAY_STRUCT_CASE(independentBlend, index, alphaOp, {           \
                    context.SetAlphaBlendOp(index, alphaOp);                           \
                })                                                                     \
                ENGINE_ARRAY_STRUCT_CASE(independentBlend, index, alphaSrcFactor, {    \
                    context.SetSrcAlphaBlendFactor(index, alphaSrcFactor);             \
                })                                                                     \
                ENGINE_ARRAY_STRUCT_CASE(independentBlend, index, alphaDstFactor, {    \
                    context.SetDstAlphaBlendFactor(index, alphaDstFactor);             \
                })

                BOOST_PP_REPEAT(8, SET_INDEPENDENT_COLOR_BLEND_CALLBACKS, 0)
                static_assert(type::RenderTargetCount == 8 && type::RenderTargetCount < BOOST_PP_LIMIT_REPEAT);
                #undef SET_COLOR_BLEND_ENABLE_CALLBACK

                #define SET_SHADER_ENABLE_CALLBACK(z, index, data)     \
                ENGINE_ARRAY_STRUCT_CASE(setProgram, index, info, { \
                    context.SetShaderEnabled(info.stage, info.enable); \
                })

                BOOST_PP_REPEAT(6, SET_SHADER_ENABLE_CALLBACK, 0)
                static_assert(type::ShaderStageCount == 6 && type::ShaderStageCount < BOOST_PP_LIMIT_REPEAT);
                #undef SET_SHADER_ENABLE_CALLBACK

                ENGINE_CASE(primitiveRestartEnable, {
                    context.SetPrimitiveRestartEnabled(primitiveRestartEnable);
                })

                ENGINE_STRUCT_CASE(constantBufferSelector, size, {
                    context.SetConstantBufferSelectorSize(size);
                })

                ENGINE_STRUCT_STRUCT_CASE(constantBufferSelector, address, high, {
                    context.SetConstantBufferSelectorIovaHigh(high);
                })

                ENGINE_STRUCT_STRUCT_CASE(constantBufferSelector, address, low, {
                    context.SetConstantBufferSelectorIovaLow(low);
                })

                ENGINE_STRUCT_STRUCT_CASE(indexBuffer, start, high, {
                    context.SetIndexBufferStartIovaHigh(high);
                })
                ENGINE_STRUCT_STRUCT_CASE(indexBuffer, start, low, {
                    context.SetIndexBufferStartIovaLow(low);
                })
                ENGINE_STRUCT_STRUCT_CASE(indexBuffer, limit, high, {
                    context.SetIndexBufferEndIovaHigh(high);
                })
                ENGINE_STRUCT_STRUCT_CASE(indexBuffer, limit, low, {
                    context.SetIndexBufferEndIovaLow(low);
                })
                ENGINE_STRUCT_CASE(indexBuffer, format, {
                    context.SetIndexBufferFormat(format);
                })

                ENGINE_CASE(bindlessTextureConstantBufferIndex, {
                    context.SetBindlessTextureConstantBufferIndex(bindlessTextureConstantBufferIndex);
                })

                ENGINE_STRUCT_STRUCT_CASE(samplerPool, address, high, {
                    context.SetSamplerPoolIovaHigh(high);
                })
                ENGINE_STRUCT_STRUCT_CASE(samplerPool, address, low, {
                    context.SetSamplerPoolIovaLow(low);
                })
                ENGINE_STRUCT_CASE(samplerPool, maximumIndex, {
                    context.SetSamplerPoolMaximumIndex(maximumIndex);
                })

                ENGINE_STRUCT_STRUCT_CASE(texturePool, address, high, {
                    context.SetTexturePoolIovaHigh(high);
                })
                ENGINE_STRUCT_STRUCT_CASE(texturePool, address, low, {
                    context.SetTexturePoolIovaLow(low);
                })
                ENGINE_STRUCT_CASE(texturePool, maximumIndex, {
                    context.SetTexturePoolMaximumIndex(maximumIndex);
                })
                ENGINE_CASE(depthMode, {
                    context.SetDepthMode(depthMode);
                })

                #define TRANSFORM_FEEDBACK_VARYINGS_CALLBACK(z, index, data)                                               \
                ENGINE_ARRAY_CASE(transformFeedbackVaryings, index, {                                                      \
                    context.SetTransformFeedbackBufferVarying(index / (type::TransformFeedbackVaryingCount / sizeof(u32)), \
                                                              index % (type::TransformFeedbackVaryingCount / sizeof(u32)), \
                                                              transformFeedbackVaryings);                                  \
                })

                BOOST_PP_REPEAT(128, TRANSFORM_FEEDBACK_VARYINGS_CALLBACK, 0)
                static_assert((type::TransformFeedbackVaryingCount / sizeof(u32)) * type::TransformFeedbackBufferCount < BOOST_PP_LIMIT_REPEAT);
                #undef TRANSFORM_FEEDBACK_VARYINGS_CALLBACK

                default:
                    break;
            }
        }

        switch (method) {
            ENGINE_STRUCT_CASE(mme, instructionRamLoad, {
                if (registers.mme->instructionRamPointer >= macroState.macroCode.size())
                    throw exception("Macro memory is full!");

                macroState.macroCode[registers.mme->instructionRamPointer++] = instructionRamLoad;

                // Wraparound writes
                // This works on HW but will also generate an error interrupt
                registers.mme->instructionRamPointer %= macroState.macroCode.size();
            })

            ENGINE_STRUCT_CASE(mme, startAddressRamLoad, {
                if (registers.mme->startAddressRamPointer >= macroState.macroPositions.size())
                    throw exception("Maximum amount of macros reached!");

                macroState.macroPositions[registers.mme->startAddressRamPointer++] = startAddressRamLoad;
            })

            ENGINE_STRUCT_CASE(i2m, launchDma, {
                i2m.LaunchDma(*registers.i2m);
            })

            ENGINE_STRUCT_CASE(i2m, loadInlineData, {
                i2m.LoadInlineData(*registers.i2m, loadInlineData);
            })

            ENGINE_CASE(syncpointAction, {
                Logger::Debug("Increment syncpoint: {}", static_cast<u16>(syncpointAction.id));
                channelCtx.executor.Submit();
                syncpoints.at(syncpointAction.id).Increment();
            })

            ENGINE_CASE(clearBuffers, {
                context.ClearBuffers(clearBuffers);
            })

            ENGINE_CASE(vertexBeginGl, {
                context.SetPrimitiveTopology(vertexBeginGl.topology);

                // If we reach here then we aren't in a deferred draw so theres no need to flush anything
                if (vertexBeginGl.instanceNext)
                    deferredDraw.instanceCount++;
                else if (vertexBeginGl.instanceContinue)
                    deferredDraw.instanceCount = 1;
            })

            ENGINE_CASE(drawVertexCount, {
                // Defer the draw until the first non-draw operation to allow for detecting instanced draws (see DeferredDrawState comment)
                deferredDraw.Set(drawVertexCount, *registers.drawVertexFirst, 0, registers.vertexBeginGl->topology, false);
            })

            ENGINE_CASE(drawIndexCount, {
                // Defer the draw until the first non-draw operation to allow for detecting instanced draws (see DeferredDrawState comment)
                deferredDraw.Set(drawIndexCount, *registers.drawIndexFirst, *registers.drawBaseVertex, registers.vertexBeginGl->topology, true);
            })

            ENGINE_STRUCT_CASE(semaphore, info, {
                if (info.reductionEnable)
                    Logger::Warn("Semaphore reduction is unimplemented!");

                switch (info.op) {
                    case type::SemaphoreInfo::Op::Release:
                        channelCtx.executor.Submit();
                        WriteSemaphoreResult(registers.semaphore->payload);
                        break;

                    case type::SemaphoreInfo::Op::Counter: {
                        switch (info.counterType) {
                            case type::SemaphoreInfo::CounterType::Zero:
                                WriteSemaphoreResult(registers.semaphore->payload);
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
                ENGINE_ARRAY_STRUCT_CASE(setProgram, index, offset, {                       \
                    context.SetShaderOffset(static_cast<type::ShaderStage>(index), offset); \
                })

            BOOST_PP_REPEAT(6, SHADER_CALLBACKS, 0)
            static_assert(type::ShaderStageCount == 6 && type::ShaderStageCount < BOOST_PP_LIMIT_REPEAT);
            #undef SHADER_CALLBACKS

            #define PIPELINE_CALLBACKS(z, idx, data)                                                                                       \
                ENGINE_ARRAY_STRUCT_CASE(bind, idx, constantBuffer, {                                                                      \
                    context.BindPipelineConstantBuffer(static_cast<type::PipelineStage>(idx), constantBuffer.valid, constantBuffer.index); \
                })

            BOOST_PP_REPEAT(5, PIPELINE_CALLBACKS, 0)
            static_assert(type::PipelineStageCount == 5 && type::PipelineStageCount < BOOST_PP_LIMIT_REPEAT);
            #undef PIPELINE_CALLBACKS

            ENGINE_ARRAY_CASE(firmwareCall, 4, {
                registers.raw[0xD00] = 1;
            })

            // Begin a batch constant buffer update, this case will never be reached if a batch update is currently active
            #define CBUF_UPDATE_CALLBACKS(z, index, data_)                                      \
            ENGINE_STRUCT_ARRAY_CASE(constantBufferUpdate, data, index, {                       \
                batchConstantBufferUpdate.startOffset = registers.constantBufferUpdate->offset; \
                batchConstantBufferUpdate.buffer.push_back(data);                               \
                registers.constantBufferUpdate->offset += 4;                                    \
            })

            BOOST_PP_REPEAT(16, CBUF_UPDATE_CALLBACKS, 0)
            #undef CBUF_UPDATE_CALLBACKS

            default:
                break;
        }
    }

    void Maxwell3D::WriteSemaphoreResult(u64 result) {
        u64 address{registers.semaphore->address};

        switch (registers.semaphore->info.structureSize) {
            case type::SemaphoreInfo::StructureSize::OneWord:
                channelCtx.asCtx->gmmu.Write(address, static_cast<u32>(result));
                Logger::Debug("address: 0x{:X} payload: {}", address, result);
                break;

            case type::SemaphoreInfo::StructureSize::FourWords: {
                // Write timestamp first to ensure correct ordering
                u64 timestamp{GetGpuTimeTicks()};
                channelCtx.asCtx->gmmu.Write(address + 8, timestamp);
                channelCtx.asCtx->gmmu.Write(address, result);
                Logger::Debug("address: 0x{:X} payload: {} timestamp: {}", address, result, timestamp);

                break;
            }
        }
    }

    void Maxwell3D::FlushEngineState() {
        FlushDeferredDraw();

        if (batchConstantBufferUpdate.Active()) {
            context.ConstantBufferUpdate(batchConstantBufferUpdate.buffer, batchConstantBufferUpdate.Invalidate());
            batchConstantBufferUpdate.Reset();
        }
    }

    __attribute__((always_inline)) void Maxwell3D::CallMethod(u32 method, u32 argument) {
        Logger::Verbose("Called method in Maxwell 3D: 0x{:X} args: 0x{:X}", method, argument);

        HandleMethod(method, argument);
    }

    void Maxwell3D::CallMethodBatchNonInc(u32 method, span<u32> arguments) {
        switch (method) {
            case ENGINE_STRUCT_OFFSET(i2m, loadInlineData):
                i2m.LoadInlineData(*registers.i2m, arguments);
                return;
            default:
                break;
        }

        for (u32 argument : arguments)
            HandleMethod(method, argument);
    }

    void Maxwell3D::CallMethodFromMacro(u32 method, u32 argument) {
        HandleMethod(method, argument);
    }

    u32 Maxwell3D::ReadMethodFromMacro(u32 method) {
        return registers.raw[method];
    }
}
