// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#include <boost/preprocessor/repeat.hpp>
#include <soc.h>

namespace skyline::soc::gm20b::engine::maxwell3d {
    Maxwell3D::Maxwell3D(const DeviceState &state) : Engine(state), macroInterpreter(*this), context(*state.gpu) {
        ResetRegs();
    }

    void Maxwell3D::ResetRegs() {
        registers = {};

        registers.rasterizerEnable = true;

        for (auto &transform : registers.viewportTransforms) {
            transform.swizzles.x = type::ViewportTransform::Swizzle::PositiveX;
            transform.swizzles.y = type::ViewportTransform::Swizzle::PositiveY;
            transform.swizzles.z = type::ViewportTransform::Swizzle::PositiveZ;
            transform.swizzles.w = type::ViewportTransform::Swizzle::PositiveW;
        }

        for (auto &viewport : registers.viewports) {
            viewport.depthRangeFar = 1.0f;
            viewport.depthRangeNear = 0.0f;
        }

        registers.polygonMode.front = type::PolygonMode::Fill;
        registers.polygonMode.back = type::PolygonMode::Fill;

        registers.stencilFront.failOp = registers.stencilFront.zFailOp = registers.stencilFront.zPassOp = type::StencilOp::Keep;
        registers.stencilFront.compare.op = type::CompareOp::Always;
        registers.stencilFront.compare.mask = 0xFFFFFFFF;
        registers.stencilFront.writeMask = 0xFFFFFFFF;

        registers.stencilTwoSideEnable = true;
        registers.stencilBack.failOp = registers.stencilBack.zFailOp = registers.stencilBack.zPassOp = type::StencilOp::Keep;
        registers.stencilBack.compareOp = type::CompareOp::Always;
        registers.stencilBackExtra.compareMask = 0xFFFFFFFF;
        registers.stencilBackExtra.writeMask = 0xFFFFFFFF;

        registers.rtSeparateFragData = true;

        for (auto &attribute : registers.vertexAttributeState)
            attribute.fixed = true;

        registers.depthTestFunc = type::CompareOp::Always;

        registers.blend.colorOp = registers.blend.alphaOp = type::Blend::Op::Add;
        registers.blend.colorSrcFactor = registers.blend.alphaSrcFactor = type::Blend::Factor::One;
        registers.blend.colorDestFactor = registers.blend.alphaDestFactor = type::Blend::Factor::Zero;

        registers.lineWidthSmooth = 1.0f;
        registers.lineWidthAliased = 1.0f;

        registers.pointSpriteEnable = true;
        registers.pointSpriteSize = 1.0f;
        registers.pointCoordReplace.enable = true;

        registers.frontFace = type::FrontFace::CounterClockwise;
        registers.cullFace = type::CullFace::Back;

        for (auto &mask : registers.colorMask)
            mask.r = mask.g = mask.b = mask.a = 1;

        for (auto &blend : registers.independentBlend) {
            blend.colorOp = blend.alphaOp = type::Blend::Op::Add;
            blend.colorSrcFactor = blend.alphaSrcFactor = type::Blend::Factor::One;
            blend.colorDestFactor = blend.alphaDestFactor = type::Blend::Factor::Zero;
        }

        registers.viewportTransformEnable = true;
    }

    void Maxwell3D::CallMethod(u32 method, u32 argument, bool lastCall) {
        state.logger->Debug("Called method in Maxwell 3D: 0x{:X} args: 0x{:X}", method, argument);

        // Methods that are greater than the register size are for macro control
        if (method >= RegisterCount) [[unlikely]] {
            // Starting a new macro at index 'method - RegisterCount'
            if (!(method & 1)) {
                if (macroInvocation.index != -1) {
                    // Flush the current macro as we are switching to another one
                    macroInterpreter.Execute(macroPositions[macroInvocation.index], macroInvocation.arguments);
                    macroInvocation.arguments.clear();
                }

                // Setup for the new macro index
                macroInvocation.index = ((method - RegisterCount) >> 1) % macroPositions.size();
            }

            macroInvocation.arguments.emplace_back(argument);

            // Flush macro after all of the data in the method call has been sent
            if (lastCall && macroInvocation.index != -1) {
                macroInterpreter.Execute(macroPositions[macroInvocation.index], macroInvocation.arguments);
                macroInvocation.arguments.clear();
                macroInvocation.index = -1;
            }

            // Bail out early
            return;
        }

        registers.raw[method] = argument;

        if (shadowRegisters.mme.shadowRamControl == type::MmeShadowRamControl::MethodTrack || shadowRegisters.mme.shadowRamControl == type::MmeShadowRamControl::MethodTrackWithFilter)
            shadowRegisters.raw[method] = argument;
        else if (shadowRegisters.mme.shadowRamControl == type::MmeShadowRamControl::MethodReplay)
            argument = shadowRegisters.raw[method];

        #define MAXWELL3D_OFFSET(field) U32_OFFSET(Registers, field)
        #define MAXWELL3D_STRUCT_OFFSET(field, member) U32_OFFSET(Registers, field) + offsetof(typeof(Registers::field), member)
        #define MAXWELL3D_ARRAY_OFFSET(field, index) U32_OFFSET(Registers, field) + ((sizeof(typeof(Registers::field[0])) / sizeof(u32)) * index)
        #define MAXWELL3D_ARRAY_STRUCT_OFFSET(field, index, member) MAXWELL3D_ARRAY_OFFSET(field, index) + U32_OFFSET(typeof(Registers::field[0]), member)
        #define MAXWELL3D_ARRAY_STRUCT_STRUCT_OFFSET(field, index, member, submember) MAXWELL3D_ARRAY_STRUCT_OFFSET(field, index, member) + U32_OFFSET(typeof(Registers::field[0].member), submember)

        switch (method) {
            case MAXWELL3D_OFFSET(mme.instructionRamLoad):
                if (registers.mme.instructionRamPointer >= macroCode.size())
                    throw exception("Macro memory is full!");

                macroCode[registers.mme.instructionRamPointer++] = argument;

                // Wraparound writes
                registers.mme.instructionRamPointer %= macroCode.size();

                break;

            case MAXWELL3D_OFFSET(mme.startAddressRamLoad):
                if (registers.mme.startAddressRamPointer >= macroPositions.size())
                    throw exception("Maximum amount of macros reached!");

                macroPositions[registers.mme.startAddressRamPointer++] = argument;
                break;

            case MAXWELL3D_OFFSET(mme.shadowRamControl):
                shadowRegisters.mme.shadowRamControl = static_cast<type::MmeShadowRamControl>(argument);
                break;

            case MAXWELL3D_OFFSET(syncpointAction):
                state.logger->Debug("Increment syncpoint: {}", static_cast<u16>(registers.syncpointAction.id));
                state.soc->host1x.syncpoints.at(registers.syncpointAction.id).Increment();
                break;

                #define RENDER_TARGET_ARRAY(z, index, data)                                 \
            case MAXWELL3D_ARRAY_STRUCT_STRUCT_OFFSET(renderTargets, index, address, high): \
                context.SetRenderTargetAddressHigh(index, argument);                 \
                break;                                                                      \
            case MAXWELL3D_ARRAY_STRUCT_STRUCT_OFFSET(renderTargets, index, address, low):  \
                context.SetRenderTargetAddressLow(index, argument);                  \
                break;                                                                      \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(renderTargets, index, width):                \
                context.SetRenderTargetAddressWidth(index, argument);                \
                break;                                                                      \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(renderTargets, index, height):               \
                context.SetRenderTargetAddressHeight(index, argument);               \
                break;                                                                      \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(renderTargets, index, format):               \
                context.SetRenderTargetAddressFormat(index,                                 \
                        static_cast<type::RenderTarget::ColorFormat>(argument));     \
                break;                                                                      \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(renderTargets, index, tileMode):             \
                context.SetRenderTargetTileMode(index,                                      \
                       *reinterpret_cast<type::RenderTarget::TileMode*>(&argument)); \
                break;                                                                      \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(renderTargets, index, arrayMode):            \
                context.SetRenderTargetArrayMode(index,                                     \
                      *reinterpret_cast<type::RenderTarget::ArrayMode*>(&argument)); \
                break;                                                                      \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(renderTargets, index, layerStrideLsr2):      \
                context.SetRenderTargetLayerStride(index, argument);                 \
                break;                                                                      \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(renderTargets, index, baseLayer):            \
                context.SetRenderTargetBaseLayer(index, argument);                   \
                break;

                BOOST_PP_REPEAT(8, RENDER_TARGET_ARRAY, 0)
                static_assert(type::RenderTargetCount == 8 && type::RenderTargetCount < BOOST_PP_LIMIT_REPEAT);
                #undef RENDER_TARGET_ARRAY

                #define VIEWPORT_TRANSFORM_CALLBACKS(z, index, data)                                                                               \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(viewportTransforms, index, scaleX):                                                        \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(viewportTransforms, index, translateX):                                                    \
                context.SetViewportX(index, registers.viewportTransforms[index].scaleX, registers.viewportTransforms[index].translateX);  \
                break;                                                                                                                    \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(viewportTransforms, index, scaleY):                                                        \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(viewportTransforms, index, translateY):                                                    \
                context.SetViewportY(index, registers.viewportTransforms[index].scaleY, registers.viewportTransforms[index].translateY);  \
                break;                                                                                                                    \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(viewportTransforms, index, scaleZ):                                                        \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(viewportTransforms, index, translateZ):                                                    \
                context.SetViewportZ(index, registers.viewportTransforms[index].scaleY, registers.viewportTransforms[index].translateY);  \
                break;

                BOOST_PP_REPEAT(16, VIEWPORT_TRANSFORM_CALLBACKS, 0)
                static_assert(type::ViewportCount == 16 && type::ViewportCount < BOOST_PP_LIMIT_REPEAT);
                #undef VIEWPORT_TRANSFORM_CALLBACKS

                #define COLOR_CLEAR_CALLBACKS(z, index, data)          \
            case MAXWELL3D_ARRAY_OFFSET(clearColorValue, index):       \
                context.UpdateClearColorValue(index, argument); \
                break;

                BOOST_PP_REPEAT(4, COLOR_CLEAR_CALLBACKS, 0)
                static_assert(4 < BOOST_PP_LIMIT_REPEAT);
                #undef COLOR_CLEAR_CALLBACKS

                #define SCISSOR_CALLBACKS(z, index, data)                                                                \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(scissors, index, enable):                                                 \
                context.SetScissor(index, argument ? registers.scissors[index] : std::optional<type::Scissor>{});        \
                break;                                                                                                   \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(scissors, index, horizontal):                                             \
                context.SetScissorHorizontal(index, registers.scissors[index].horizontal);                               \
                break;                                                                                                   \
            case MAXWELL3D_ARRAY_STRUCT_OFFSET(scissors, index, vertical):                                               \
                context.SetScissorVertical(index, registers.scissors[index].vertical);                                   \
                break;

                BOOST_PP_REPEAT(16, SCISSOR_CALLBACKS, 0)
                static_assert(type::ViewportCount == 16 && type::ViewportCount < BOOST_PP_LIMIT_REPEAT);
                #undef SCISSOR_CALLBACKS

            case MAXWELL3D_OFFSET(renderTargetControl):
                context.UpdateRenderTargetControl(registers.renderTargetControl);
                break;

            case MAXWELL3D_OFFSET(clearBuffers):
                context.ClearBuffers(registers.clearBuffers);
                break;

            case MAXWELL3D_OFFSET(semaphore.info):
                switch (registers.semaphore.info.op) {
                    case type::SemaphoreInfo::Op::Release:
                        WriteSemaphoreResult(registers.semaphore.payload);
                        break;

                    case type::SemaphoreInfo::Op::Counter: {
                        switch (registers.semaphore.info.counterType) {
                            case type::SemaphoreInfo::CounterType::Zero:
                                WriteSemaphoreResult(0);
                                break;

                            default:
                                state.logger->Warn("Unsupported semaphore counter type: 0x{:X}", static_cast<u8>(registers.semaphore.info.counterType));
                                break;
                        }
                        break;
                    }

                    default:
                        state.logger->Warn("Unsupported semaphore operation: 0x{:X}", static_cast<u8>(registers.semaphore.info.op));
                        break;
                }
                break;

            case MAXWELL3D_OFFSET(firmwareCall[4]):
                registers.raw[0xD00] = 1;
                break;
            default:
                break;
        }

        #undef MAXWELL3D_OFFSET
        #undef MAXWELL3D_STRUCT_OFFSET
        #undef MAXWELL3D_ARRAY_OFFSET
        #undef MAXWELL3D_ARRAY_STRUCT_OFFSET
        #undef MAXWELL3D_ARRAY_STRUCT_STRUCT_OFFSET
    }

    void Maxwell3D::WriteSemaphoreResult(u64 result) {
        struct FourWordResult {
            u64 value;
            u64 timestamp;
        };

        switch (registers.semaphore.info.structureSize) {
            case type::SemaphoreInfo::StructureSize::OneWord:
                state.soc->gm20b.gmmu.Write<u32>(registers.semaphore.address.Pack(), static_cast<u32>(result));
                break;

            case type::SemaphoreInfo::StructureSize::FourWords: {
                // Convert the current nanosecond time to GPU ticks
                constexpr u64 NsToTickNumerator{384};
                constexpr u64 NsToTickDenominator{625};

                u64 nsTime{util::GetTimeNs()};
                u64 timestamp{(nsTime / NsToTickDenominator) * NsToTickNumerator + ((nsTime % NsToTickDenominator) * NsToTickNumerator) / NsToTickDenominator};

                state.soc->gm20b.gmmu.Write<FourWordResult>(registers.semaphore.address.Pack(), FourWordResult{result, timestamp});
                break;
            }
        }
    }
}
