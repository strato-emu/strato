// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <gpu/syncpoint.h>
#include "maxwell_3d.h"

namespace skyline::gpu::engine {
    Maxwell3D::Maxwell3D(const DeviceState &state) : Engine(state), macroInterpreter(*this) {
        ResetRegs();
    }

    void Maxwell3D::ResetRegs() {
        memset(&regs, 0, sizeof(regs));

        regs.rasterizerEnable = true;

        for (auto &transform : regs.viewportTransform) {
            transform.swizzles.x = Regs::ViewportTransform::Swizzle::PositiveX;
            transform.swizzles.y = Regs::ViewportTransform::Swizzle::PositiveY;
            transform.swizzles.z = Regs::ViewportTransform::Swizzle::PositiveZ;
            transform.swizzles.w = Regs::ViewportTransform::Swizzle::PositiveW;
        }

        for (auto &viewport : regs.viewport) {
            viewport.depthRangeFar = 1.0f;
            viewport.depthRangeNear = 0.0f;
        }

        regs.polygonMode.front = Regs::PolygonMode::Fill;
        regs.polygonMode.back = Regs::PolygonMode::Fill;

        regs.stencilFront.failOp = regs.stencilFront.zFailOp = regs.stencilFront.zPassOp = Regs::StencilOp::Keep;
        regs.stencilFront.func.op = Regs::CompareOp::Always;
        regs.stencilFront.func.mask = 0xFFFFFFFF;
        regs.stencilFront.mask = 0xFFFFFFFF;

        regs.stencilBack.stencilTwoSideEnable = true;
        regs.stencilBack.failOp = regs.stencilBack.zFailOp = regs.stencilBack.zPassOp = Regs::StencilOp::Keep;
        regs.stencilBack.funcOp = Regs::CompareOp::Always;
        regs.stencilBackExtra.funcMask = 0xFFFFFFFF;
        regs.stencilBackExtra.mask = 0xFFFFFFFF;

        regs.rtSeparateFragData = true;

        for (auto &attribute : regs.vertexAttributeState)
            attribute.fixed = true;

        regs.depthTestFunc = Regs::CompareOp::Always;

        regs.blend.colorOp = regs.blend.alphaOp = Regs::Blend::Op::Add;
        regs.blend.colorSrcFactor = regs.blend.alphaSrcFactor = Regs::Blend::Factor::One;
        regs.blend.colorDestFactor = regs.blend.alphaDestFactor = Regs::Blend::Factor::Zero;

        regs.lineWidthSmooth = 1.0f;
        regs.lineWidthAliased = 1.0f;

        regs.pointSpriteSize = 1.0f;

        regs.frontFace = Regs::FrontFace::CounterClockwise;
        regs.cullFace = Regs::CullFace::Back;

        for (auto &mask : regs.colorMask)
            mask.r = mask.g = mask.b = mask.a = 1;

        for (auto &blend : regs.independentBlend) {
            blend.colorOp = blend.alphaOp = Regs::Blend::Op::Add;
            blend.colorSrcFactor = blend.alphaSrcFactor = Regs::Blend::Factor::One;
            blend.colorDestFactor = blend.alphaDestFactor = Regs::Blend::Factor::Zero;
        }
    }

    void Maxwell3D::CallMethod(MethodParams params) {
        state.logger->Debug("Called method in Maxwell 3D: 0x{:X} args: 0x{:X}", params.method, params.argument);

        // Methods that are greater than the register size are for macro control
        if (params.method > constant::Maxwell3DRegisterSize) {
            if (!(params.method & 1))
                macroInvocation.index = ((params.method - constant::Maxwell3DRegisterSize) >> 1) % macroPositions.size();

            macroInvocation.arguments.push_back(params.argument);

            // Macros are always executed on the last method call in a pushbuffer entry
            if (params.lastCall) {
                macroInterpreter.Execute(macroPositions[macroInvocation.index], macroInvocation.arguments);

                macroInvocation.arguments.clear();
                macroInvocation.index = 0;
            }
            return;
        }

        regs.raw[params.method] = params.argument;

        if (shadowRegs.mme.shadowRamControl == Regs::MmeShadowRamControl::MethodTrack || shadowRegs.mme.shadowRamControl == Regs::MmeShadowRamControl::MethodTrackWithFilter)
            shadowRegs.raw[params.method] = params.argument;
        else if (shadowRegs.mme.shadowRamControl == Regs::MmeShadowRamControl::MethodReplay)
            params.argument = shadowRegs.raw[params.method];

        switch (params.method) {
            case MAXWELL3D_OFFSET(mme.instructionRamLoad):
                if (regs.mme.instructionRamPointer >= macroCode.size())
                    throw exception("Macro memory is full!");

                macroCode[regs.mme.instructionRamPointer++] = params.argument;
                break;
            case MAXWELL3D_OFFSET(mme.startAddressRamLoad):
                if (regs.mme.startAddressRamPointer >= macroPositions.size())
                    throw exception("Maximum amount of macros reached!");

                macroPositions[regs.mme.startAddressRamPointer++] = params.argument;
                break;
            case MAXWELL3D_OFFSET(mme.shadowRamControl):
                shadowRegs.mme.shadowRamControl = static_cast<Regs::MmeShadowRamControl>(params.argument);
                break;
            case MAXWELL3D_OFFSET(syncpointAction):
                state.gpu->syncpoints.at(regs.syncpointAction.id).Increment();
                break;
            case MAXWELL3D_OFFSET(semaphore.info):
                switch (regs.semaphore.info.op) {
                    case Regs::SemaphoreInfo::Op::Release:
                        WriteSemaphoreResult(regs.semaphore.payload);
                        break;
                    case Regs::SemaphoreInfo::Op::Counter:
                        HandleSemaphoreCounterOperation();
                        break;
                    default:
                        state.logger->Warn("Unsupported semaphore operation: 0x{:X}", static_cast<u8>(regs.semaphore.info.op));
                        break;
                }
                break;
            case MAXWELL3D_OFFSET(firmwareCall[4]):
                regs.raw[0xd00] = 1;
                break;
        }
    }

    void Maxwell3D::HandleSemaphoreCounterOperation() {
        switch (regs.semaphore.info.counterType) {
            case Regs::SemaphoreInfo::CounterType::Zero:
                WriteSemaphoreResult(0);
                break;
            default:
                state.logger->Warn("Unsupported semaphore counter type: 0x{:X}", static_cast<u8>(regs.semaphore.info.counterType));
                break;
        }
    }

    void Maxwell3D::WriteSemaphoreResult(u64 result) {
        struct FourWordResult {
            u64 value;
            u64 timestamp;
        };

        switch (regs.semaphore.info.structureSize) {
            case Regs::SemaphoreInfo::StructureSize::OneWord:
                state.gpu->memoryManager.Write<u32>(static_cast<u32>(result), regs.semaphore.address.Pack());
                break;
            case Regs::SemaphoreInfo::StructureSize::FourWords: {
                // Convert the current nanosecond time to GPU ticks
                constexpr u64 NsToTickNumerator = 384;
                constexpr u64 NsToTickDenominator = 625;

                u64 nsTime = util::GetTimeNs();
                u64 timestamp = (nsTime / NsToTickDenominator) * NsToTickNumerator + ((nsTime % NsToTickDenominator) * NsToTickNumerator) / NsToTickDenominator;

                state.gpu->memoryManager.Write<FourWordResult>(FourWordResult{result, timestamp}, regs.semaphore.address.Pack());
                break;
            }
        }
    }
}
