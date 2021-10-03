// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/signal.h>
#include <loader/loader.h>
#include <kernel/types/KProcess.h>
#include <soc.h>
#include <os.h>

namespace skyline::soc::gm20b {
    /**
     * @brief A single pushbuffer method header that describes a compressed method sequence
     * @url https://github.com/NVIDIA/open-gpu-doc/blob/ab27fc22db5de0d02a4cabe08e555663b62db4d4/manuals/volta/gv100/dev_ram.ref.txt#L850
     * @url https://github.com/NVIDIA/open-gpu-doc/blob/ab27fc22db5de0d02a4cabe08e555663b62db4d4/classes/host/clb06f.h#L179
     */
    union PushBufferMethodHeader {
        u32 raw;

        enum class TertOp : u8 {
            Grp0IncMethod = 0,
            Grp0SetSubDevMask = 1,
            Grp0StoreSubDevMask = 2,
            Grp0UseSubDevMask = 3,
            Grp2NonIncMethod = 0,
        };

        enum class SecOp : u8 {
            Grp0UseTert = 0,
            IncMethod = 1,
            Grp2UseTert = 2,
            NonIncMethod = 3,
            ImmdDataMethod = 4,
            OneInc = 5,
            Reserved6 = 6,
            EndPbSegment = 7,
        };

        u16 methodAddress : 12;
        struct {
            u8 _pad0_ : 4;
            u16 subDeviceMask : 12;
        };

        struct {
            u16 _pad1_ : 13;
            u8 methodSubChannel : 3;
            union {
                TertOp tertOp : 3;
                u16 methodCount : 13;
                u16 immdData : 13;
            };
        };

        struct {
            u32 _pad2_ : 29;
            SecOp secOp : 3;
        };
    };
    static_assert(sizeof(PushBufferMethodHeader) == sizeof(u32));

    void GPFIFO::Send(u32 method, u32 argument, u32 subChannel, bool lastCall) {
        constexpr u32 ThreeDSubChannel{0};
        constexpr u32 ComputeSubChannel{1};
        constexpr u32 Inline2MemorySubChannel{2};
        constexpr u32 TwoDSubChannel{3};
        constexpr u32 CopySubChannel{4}; // HW forces a memory flush on a switch from this subchannel to others

        state.logger->Debug("Called GPU method - method: 0x{:X} argument: 0x{:X} subchannel: 0x{:X} last: {}", method, argument, subChannel, lastCall);

        if (method < engine::GPFIFO::RegisterCount) {
            gpfifoEngine.CallMethod(method, argument, lastCall);
        } else {
            switch (subChannel) {
                case ThreeDSubChannel:
                    state.soc->gm20b.maxwell3D.CallMethod(method, argument, lastCall);
                    break;
                case ComputeSubChannel:
                    state.soc->gm20b.maxwellCompute.CallMethod(method, argument, lastCall);
                    break;
                case Inline2MemorySubChannel:
                    state.soc->gm20b.keplerMemory.CallMethod(method, argument, lastCall);
                    break;
                case TwoDSubChannel:
                    state.soc->gm20b.fermi2D.CallMethod(method, argument, lastCall);
                    break;
                case CopySubChannel:
                    state.soc->gm20b.maxwellDma.CallMethod(method, argument, lastCall);
                    break;
                default:
                    throw exception("Tried to call into a software subchannel: {}!", subChannel);
            }
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
        state.soc->gm20b.gmmu.Read<u32>(pushBufferData, gpEntry.Address());

        for (auto entry{pushBufferData.begin()}; entry != pushBufferData.end(); entry++) {
            // An entry containing all zeroes is a NOP, skip over it
            if (*entry == 0)
                continue;

            PushBufferMethodHeader methodHeader{.raw = *entry};
            switch (methodHeader.secOp) {
                case PushBufferMethodHeader::SecOp::IncMethod:
                    for (u32 i{}; i < methodHeader.methodCount; i++)
                        Send(methodHeader.methodAddress + i, *++entry, methodHeader.methodSubChannel, i == methodHeader.methodCount - 1);
                    break;

                case PushBufferMethodHeader::SecOp::NonIncMethod:
                    for (u32 i{}; i < methodHeader.methodCount; i++)
                        Send(methodHeader.methodAddress, *++entry, methodHeader.methodSubChannel, i == methodHeader.methodCount - 1);
                    break;

                case PushBufferMethodHeader::SecOp::OneInc:
                    for (u32 i{}; i < methodHeader.methodCount; i++)
                        Send(methodHeader.methodAddress + !!i, *++entry, methodHeader.methodSubChannel, i == methodHeader.methodCount - 1);
                    break;

                case PushBufferMethodHeader::SecOp::ImmdDataMethod:
                    Send(methodHeader.methodAddress, methodHeader.immdData, methodHeader.methodSubChannel, true);
                    break;

                case PushBufferMethodHeader::SecOp::EndPbSegment:
                    return;

                default:
                    throw exception("Unsupported pushbuffer method SecOp: {}", static_cast<u8>(methodHeader.secOp));
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
