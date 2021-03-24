// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/signal.h>
#include <loader/loader.h>
#include <kernel/types/KProcess.h>
#include <soc.h>

namespace skyline::soc::gm20b {
    void GPFIFO::Send(MethodParams params) {
        state.logger->Debug("Called GPU method - method: 0x{:X} argument: 0x{:X} subchannel: 0x{:X} last: {}", params.method, params.argument, params.subChannel, params.lastCall);

        if (params.method == 0) {
            switch (static_cast<EngineID>(params.argument)) {
                case EngineID::Fermi2D:
                    subchannels.at(params.subChannel) = &state.soc->gm20b.fermi2D;
                    break;
                case EngineID::KeplerMemory:
                    subchannels.at(params.subChannel) = &state.soc->gm20b.keplerMemory;
                    break;
                case EngineID::Maxwell3D:
                    subchannels.at(params.subChannel) = &state.soc->gm20b.maxwell3D;
                    break;
                case EngineID::MaxwellCompute:
                    subchannels.at(params.subChannel) = &state.soc->gm20b.maxwellCompute;
                    break;
                case EngineID::MaxwellDma:
                    subchannels.at(params.subChannel) = &state.soc->gm20b.maxwellDma;
                    break;
                default:
                    throw exception("Unknown engine 0x{:X} cannot be bound to subchannel {}", params.argument, params.subChannel);
            }

            state.logger->Info("Bound GPU engine 0x{:X} to subchannel {}", params.argument, params.subChannel);
            return;
        } else if (params.method < engine::GPFIFO::RegisterCount) {
            gpfifoEngine.CallMethod(params);
        } else {
            if (subchannels.at(params.subChannel) == nullptr)
                throw exception("Calling method on unbound channel");

            subchannels.at(params.subChannel)->CallMethod(params);
        }
    }

    void GPFIFO::Process(GpEntry gpEntry) {
        if (!gpEntry.size) {
            // This is a GPFIFO control entry, all control entries have a zero length and contain no pushbuffers
            switch (gpEntry.opcode) {
                case GpEntry::Opcode::Nop:
                    return;
                default:
                    state.logger->Warn("Unsupported GpEntry control opcode used: {}", static_cast<u8>(gpEntry.opcode));
                    return;
            }
        }

        pushBufferData.resize(gpEntry.size);
        state.soc->gmmu.Read<u32>(pushBufferData, gpEntry.Address());

        for (auto entry{pushBufferData.begin()}; entry != pushBufferData.end(); entry++) {
            // An entry containing all zeroes is a NOP, skip over it
            if (*entry == 0)
                continue;

            PushBufferMethodHeader methodHeader{.raw = *entry};
            switch (methodHeader.secOp) {
                case PushBufferMethodHeader::SecOp::IncMethod:
                    for (u16 i{}; i < methodHeader.methodCount; i++)
                        Send(MethodParams{static_cast<u16>(methodHeader.methodAddress + i), *++entry, methodHeader.methodSubChannel, i == methodHeader.methodCount - 1});
                    break;

                case PushBufferMethodHeader::SecOp::NonIncMethod:
                    for (u16 i{}; i < methodHeader.methodCount; i++)
                        Send(MethodParams{methodHeader.methodAddress, *++entry, methodHeader.methodSubChannel, i == methodHeader.methodCount - 1});
                    break;

                case PushBufferMethodHeader::SecOp::OneInc:
                    for (u16 i{}; i < methodHeader.methodCount; i++)
                        Send(MethodParams{static_cast<u16>(methodHeader.methodAddress + static_cast<bool>(i)), *++entry, methodHeader.methodSubChannel, i == methodHeader.methodCount - 1});
                    break;

                case PushBufferMethodHeader::SecOp::ImmdDataMethod:
                    Send(MethodParams{methodHeader.methodAddress, methodHeader.immdData, methodHeader.methodSubChannel, true});
                    break;

                case PushBufferMethodHeader::SecOp::EndPbSegment:
                    return;

                default:
                    state.logger->Warn("Unsupported pushbuffer method SecOp: {}", static_cast<u8>(methodHeader.secOp));
                    break;
            }
        }
    }

    void GPFIFO::Initialize(size_t numBuffers) {
        if (pushBuffers)
            throw exception("GPFIFO Initialization cannot be done multiple times");
        pushBuffers.emplace(numBuffers);
        thread = std::thread(&GPFIFO::Run, this);
    }

    void GPFIFO::Run() {
        pthread_setname_np(pthread_self(), "GPFIFO");
        try {
            signal::SetSignalHandler({SIGINT, SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV}, signal::ExceptionalSignalHandler);
            pushBuffers->Process([this](GpEntry gpEntry) {
                state.logger->Debug("Processing pushbuffer: 0x{:X}", gpEntry.Address());
                Process(gpEntry);
            });
        } catch (const signal::SignalException &e) {
            if (e.signal != SIGINT) {
                state.logger->Error("{}\nStack Trace:{}", e.what(), state.loader->GetStackTrace(e.frames));
                signal::BlockSignal({SIGINT});
                state.process->Kill(false);
            }
        } catch (const std::exception &e) {
            state.logger->Error(e.what());
            signal::BlockSignal({SIGINT});
            state.process->Kill(false);
        }
    }

    void GPFIFO::Push(span<GpEntry> entries) {
        pushBuffers->Append(entries);
    }

    GPFIFO::~GPFIFO() {
        if (thread.joinable()) {
            pthread_kill(thread.native_handle(), SIGINT);
            thread.join();
        }
    }
}
