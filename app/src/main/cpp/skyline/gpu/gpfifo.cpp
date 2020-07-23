// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include "gpfifo.h"

namespace skyline::gpu::gpfifo {
    void GPFIFO::Send(MethodParams params) {
        state.logger->Warn("Called unimplemented GPU method - method: 0x{:X} argument: 0x{:X} subchannel: 0x{:X} last: {}", params.method, params.argument, params.subChannel, params.lastCall);
    }

    void GPFIFO::Process(const std::vector<u32> &segment) {
        for (auto entry = segment.begin(); entry != segment.end(); entry++) {
            auto methodHeader = reinterpret_cast<const PushBufferMethodHeader *>(&*entry);

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
                        Send(MethodParams{static_cast<u16>(methodHeader->methodAddress + bool(i)), *++entry, methodHeader->methodSubChannel, i == methodHeader->methodCount - 1});

                    break;
                case PushBufferMethodHeader::SecOp::ImmdDataMethod:
                    Send(MethodParams{methodHeader->methodAddress, methodHeader->immdData, methodHeader->methodSubChannel, true});
                    break;
                default:
                    break;
            }
        }
    }

    void GPFIFO::Run() {
        std::lock_guard lock(pushBufferQueueLock);
        while (!pushBufferQueue.empty()) {
            auto pushBuffer = pushBufferQueue.front();
            if (pushBuffer.segment.empty())
                pushBuffer.Fetch(state.gpu->memoryManager);

            Process(pushBuffer.segment);
            pushBufferQueue.pop();
        }
    }

    void GPFIFO::Push(std::span<GpEntry> entries) {
        std::lock_guard lock(pushBufferQueueLock);
        bool beforeBarrier{true};

        for (const auto &entry : entries) {
            if (entry.sync == GpEntry::Sync::Wait)
                beforeBarrier = false;

            pushBufferQueue.emplace(PushBuffer(entry, state.gpu->memoryManager, beforeBarrier));
        }
    }
}
