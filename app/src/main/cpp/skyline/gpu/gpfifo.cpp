// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <gpu/engines/maxwell_3d.h>
#include "gpfifo.h"

namespace skyline::gpu::gpfifo {
    void GPFIFO::Send(MethodParams params) {
        state.logger->Debug("Called GPU method - method: 0x{:X} argument: 0x{:X} subchannel: 0x{:X} last: {}", params.method, params.argument, params.subChannel, params.lastCall);

        if (params.method == 0) {
            switch (static_cast<EngineID>(params.argument)) {
                case EngineID::Fermi2D:
                    subchannels.at(params.subChannel) = state.gpu->fermi2D;
                    break;
                case EngineID::KeplerMemory:
                    subchannels.at(params.subChannel) = state.gpu->keplerMemory;
                    break;
                case EngineID::Maxwell3D:
                    subchannels.at(params.subChannel) = state.gpu->maxwell3D;
                    break;
                case EngineID::MaxwellCompute:
                    subchannels.at(params.subChannel) = state.gpu->maxwellCompute;
                    break;
                case EngineID::MaxwellDma:
                    subchannels.at(params.subChannel) = state.gpu->maxwellDma;
                    break;
                default:
                    throw exception("Unknown engine 0x{:X} cannot be bound to subchannel {}", params.argument, params.subChannel);
            }

            state.logger->Info("Bound GPU engine 0x{:X} to subchannel {}", params.argument, params.subChannel);
            return;
        } else if (params.method < constant::GpfifoRegisterCount) {
            gpfifoEngine.CallMethod(params);
        } else {
            if (subchannels.at(params.subChannel) == nullptr)
                throw exception("Calling method on unbound channel");

            subchannels.at(params.subChannel)->CallMethod(params);
        }
    }

    void GPFIFO::Process(const std::vector<u32> &segment) {
        for (auto entry{segment.begin()}; entry != segment.end(); entry++) {
            // An entry containing all zeroes is a NOP, skip over it
            if (*entry == 0)
                continue;

            auto methodHeader{reinterpret_cast<const PushBufferMethodHeader *>(&*entry)};

            switch (methodHeader->secOp) {
                case PushBufferMethodHeader::SecOp::IncMethod:
                    for (u16 i{}; i < methodHeader->methodCount; i++)
                        Send(MethodParams{static_cast<u16>(methodHeader->methodAddress + i), *++entry, methodHeader->methodSubChannel, i == methodHeader->methodCount - 1});

                    break;
                case PushBufferMethodHeader::SecOp::NonIncMethod:
                    for (u16 i{}; i < methodHeader->methodCount; i++)
                        Send(MethodParams{methodHeader->methodAddress, *++entry, methodHeader->methodSubChannel, i == methodHeader->methodCount - 1});

                    break;
                case PushBufferMethodHeader::SecOp::OneInc:
                    for (u16 i{}; i < methodHeader->methodCount; i++)
                        Send(MethodParams{static_cast<u16>(methodHeader->methodAddress + static_cast<bool>(i)), *++entry, methodHeader->methodSubChannel, i == methodHeader->methodCount - 1});

                    break;
                case PushBufferMethodHeader::SecOp::ImmdDataMethod:
                    Send(MethodParams{methodHeader->methodAddress, methodHeader->immdData, methodHeader->methodSubChannel, true});
                    break;
                case PushBufferMethodHeader::SecOp::EndPbSegment:
                    return;
                default:
                    break;
            }
        }
    }

    void GPFIFO::Run() {
        std::lock_guard lock(pushBufferQueueLock);
        while (!pushBufferQueue.empty()) {
            auto pushBuffer{pushBufferQueue.front()};
            if (pushBuffer.segment.empty())
                pushBuffer.Fetch(state.gpu->memoryManager);

            Process(pushBuffer.segment);
            pushBufferQueue.pop();
        }
    }

    void GPFIFO::Push(span<GpEntry> entries) {
        std::lock_guard lock(pushBufferQueueLock);
        bool beforeBarrier{false};

        for (const auto &entry : entries) {
            if (entry.sync == GpEntry::Sync::Wait)
                beforeBarrier = false;

            pushBufferQueue.emplace(PushBuffer(entry, state.gpu->memoryManager, beforeBarrier));
        }
    }
}
