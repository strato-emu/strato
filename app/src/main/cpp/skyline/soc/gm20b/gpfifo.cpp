// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/signal.h>
#include <loader/loader.h>
#include <kernel/types/KProcess.h>
#include <soc.h>
#include <os.h>
#include "engines/maxwell_3d.h"
#include "engines/fermi_2d.h"

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
            SubchannelId methodSubChannel : 3;
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

        /**
         * @brief Checks if a method is 'pure' i.e. does not touch macro or GPFIFO methods
         */
        bool Pure() const {
            u16 size{[&]() -> u16  {
                switch (secOp) {
                    case SecOp::NonIncMethod:
                    case SecOp::ImmdDataMethod:
                        return 0;
                    case SecOp::OneInc:
                        return 1;
                    default:
                        return methodCount;
                }
            }()};

            u16 end{static_cast<u16>(methodAddress + size)};
            return end < engine::EngineMethodsEnd && methodAddress >= engine::GPFIFO::RegisterCount;
        }
    };
    static_assert(sizeof(PushBufferMethodHeader) == sizeof(u32));

    ChannelGpfifo::ChannelGpfifo(const DeviceState &state, ChannelContext &channelCtx, size_t numEntries) :
        state(state),
        gpfifoEngine(state.soc->host1x.syncpoints, channelCtx),
        channelCtx(channelCtx),
        gpEntries(numEntries),
        thread(std::thread(&ChannelGpfifo::Run, this)) {}

    void ChannelGpfifo::SendFull(u32 method, u32 argument, SubchannelId subChannel, bool lastCall) {
        if (method < engine::GPFIFO::RegisterCount) {
            gpfifoEngine.CallMethod(method, argument);
        } else if (method < engine::EngineMethodsEnd) { [[likely]]
            SendPure(method, argument, subChannel);
        } else {
            switch (subChannel) {
                case SubchannelId::ThreeD:
                    channelCtx.maxwell3D->HandleMacroCall(method - engine::EngineMethodsEnd, argument, lastCall);
                    break;
                case SubchannelId::TwoD:
                    channelCtx.fermi2D.HandleMacroCall(method - engine::EngineMethodsEnd, argument, lastCall);
                    break;
                default:
                    Logger::Warn("Called method 0x{:X} out of bounds for engine 0x{:X}, args: 0x{:X}", method, subChannel, argument);
                    break;
            }
        }
    }

    void ChannelGpfifo::SendPure(u32 method, u32 argument, SubchannelId subChannel) {
        switch (subChannel) {
            case SubchannelId::ThreeD:
                channelCtx.maxwell3D->CallMethod(method, argument);
                break;
            case SubchannelId::Compute:
                channelCtx.keplerCompute.CallMethod(method, argument);
                break;
            case SubchannelId::Inline2Mem:
                channelCtx.inline2Memory.CallMethod(method, argument);
                break;
            case SubchannelId::Copy:
                channelCtx.maxwellDma.CallMethod(method, argument);
            case SubchannelId::TwoD:
                channelCtx.fermi2D.CallMethod(method, argument);
                break;
            default:
                Logger::Warn("Called method 0x{:X} in unimplemented engine 0x{:X}, args: 0x{:X}", method, subChannel, argument);
                break;
        }
    }

    void ChannelGpfifo::SendPureBatchNonInc(u32 method, span<u32> arguments, SubchannelId subChannel) {
        switch (subChannel) {
            case SubchannelId::ThreeD:
                channelCtx.maxwell3D->CallMethodBatchNonInc(method, arguments);
                break;
            case SubchannelId::Compute:
                channelCtx.keplerCompute.CallMethodBatchNonInc(method, arguments);
                break;
            case SubchannelId::Inline2Mem:
                channelCtx.inline2Memory.CallMethodBatchNonInc(method, arguments);
                break;
            case SubchannelId::Copy:
                channelCtx.maxwellDma.CallMethodBatchNonInc(method, arguments);
                break;
            default:
                Logger::Warn("Called method 0x{:X} in unimplemented engine 0x{:X} with batch args", method, subChannel);
                break;
        }
    }

    void ChannelGpfifo::Process(GpEntry gpEntry) {
        // Submit if required by the GpEntry, this is needed as some games dynamically generate pushbuffer contents
        if (gpEntry.sync == GpEntry::Sync::Wait)
            channelCtx.executor.Submit();

        if (!gpEntry.size) {
            // This is a GPFIFO control entry, all control entries have a zero length and contain no pushbuffers
            switch (gpEntry.opcode) {
                case GpEntry::Opcode::Nop:
                    return;
                default:
                    Logger::Warn("Unsupported GpEntry control opcode used: {}", static_cast<u8>(gpEntry.opcode));
                    return;
            }
        }

        pushBufferData.resize(gpEntry.size);
        channelCtx.asCtx->gmmu.Read<u32>(pushBufferData, gpEntry.Address());

        // There will be at least one entry here
        auto entry{pushBufferData.begin()};

        // Executes the current split method, returning once execution is finished or the current GpEntry has reached its end
        auto resumeSplitMethod{[&](){
            switch (resumeState.state) {
                case MethodResumeState::State::Inc:
                    while (entry != pushBufferData.end() && resumeState.remaining)
                        SendFull(resumeState.address++, *(entry++), resumeState.subChannel, --resumeState.remaining == 0);

                    break;
                case MethodResumeState::State::OneInc:
                    SendFull(resumeState.address++, *(entry++), resumeState.subChannel, --resumeState.remaining == 0);

                    // After the first increment OneInc methods work the same as a NonInc method, this is needed so they can resume correctly if they are broken up by multiple GpEntries
                    resumeState.state = MethodResumeState::State::NonInc;
                    [[fallthrough]];
                case MethodResumeState::State::NonInc:
                    while (entry != pushBufferData.end() && resumeState.remaining)
                        SendFull(resumeState.address, *(entry++), resumeState.subChannel, --resumeState.remaining == 0);

                    break;
            }
        }};

        // We've a method from a previous GpEntry that needs resuming
        if (resumeState.remaining)
            resumeSplitMethod();

        // Process more methods if the entries are still not all used up after handling resuming
        for (; entry != pushBufferData.end(); entry++) {
            if (entry >= pushBufferData.end())
                throw exception("GPFIFO buffer overflow!"); // This should never happen

            // An entry containing all zeroes is a NOP, skip over it
            if (*entry == 0)
                continue;

            PushBufferMethodHeader methodHeader{.raw = *entry};

            // Needed in order to check for methods split across multiple GpEntries
            ssize_t remainingEntries{std::distance(entry, pushBufferData.end()) - 1};

            // Handles storing state and initial execution for methods that are split across multiple GpEntries
            auto startSplitMethod{[&](auto methodState) {
                resumeState = {
                    .remaining = methodHeader.methodCount,
                    .address = methodHeader.methodAddress,
                    .subChannel = methodHeader.methodSubChannel,
                    .state = methodState
                };

                // Skip over method header as `resumeSplitMethod` doesn't expect it to be there
                entry++;

                resumeSplitMethod();
            }};

            /**
             * @brief Handles execution of a specific method type as specified by the State template parameter
             * @tparam ThreeDOnly Whether to skip subchannel method handling and send all method calls to the 3D engine
             */
            auto dispatchCalls{[&]<bool ThreeDOnly, MethodResumeState::State State> () {
                /**
                 * @brief Gets the offset to apply to the method address for a given dispatch loop index
                 */
                auto methodOffset{[] (u32 i) -> u32 {
                    switch (State)  {
                        case MethodResumeState::State::Inc:
                            return i;
                        case MethodResumeState::State::OneInc:
                            return i ? 1 : 0;
                        case MethodResumeState::State::NonInc:
                            return 0;
                    }
                }};

                constexpr u32 BatchCutoff{4}; //!< Cutoff needed to send method calls in a batch which is espcially important for UBO updates. This helps to avoid the extra overhead batching for small packets.
                // TODO: Only batch for specific target methods like UBO updates, since normal dispatch is generally cheaper

                if (remainingEntries >= methodHeader.methodCount) {
                    if (methodHeader.Pure()) [[likely]] {
                        if constexpr (State == MethodResumeState::State::NonInc) {
                            // For pure noninc methods we can send all method calls as a span in one go
                            if (methodHeader.methodCount > BatchCutoff) {
                                if constexpr (ThreeDOnly)
                                    channelCtx.maxwell3D->CallMethodBatchNonInc(methodHeader.methodAddress, span<u32>(&(*++entry), methodHeader.methodCount));
                                else
                                    SendPureBatchNonInc(methodHeader.methodAddress, span(&(*++entry), methodHeader.methodCount), methodHeader.methodSubChannel);

                                entry += methodHeader.methodCount - 1;
                                return false;
                            }
                        } else if constexpr (State == MethodResumeState::State::OneInc) {
                            // For pure oneinc methods we can send the initial method then send the rest as a span in one go
                            if (methodHeader.methodCount > (BatchCutoff + 1)) {
                                if constexpr (ThreeDOnly) {
                                    channelCtx.maxwell3D->CallMethod(methodHeader.methodAddress, *++entry);
                                    channelCtx.maxwell3D->CallMethodBatchNonInc(methodHeader.methodAddress + 1, span(&(*++entry), methodHeader.methodCount - 1));
                                } else {
                                    SendPure(methodHeader.methodAddress, *++entry, methodHeader.methodSubChannel);
                                    SendPureBatchNonInc(methodHeader.methodAddress + 1, span(&(*++entry) ,methodHeader.methodCount - 1), methodHeader.methodSubChannel);
                                }

                                entry += methodHeader.methodCount - 2;
                                return false;
                            }
                        }

                        for (u32 i{}; i < methodHeader.methodCount; i++) {
                            if constexpr (ThreeDOnly) {
                                channelCtx.maxwell3D->CallMethod(methodHeader.methodAddress + methodOffset(i), *++entry);
                            } else {
                                SendPure(methodHeader.methodAddress + methodOffset(i), *++entry, methodHeader.methodSubChannel);
                            }
                        }
                    } else {
                        // Slow path for methods that touch GPFIFO or macros
                        for (u32 i{}; i < methodHeader.methodCount; i++)
                            SendFull(methodHeader.methodAddress + methodOffset(i), *++entry, methodHeader.methodSubChannel, i == methodHeader.methodCount - 1);
                    }
                } else {
                    startSplitMethod(State);
                    return true;
                }

                return false;
            }};

            /**
             * @brief Handles execution of a single method
             * @tparam ThreeDOnly Whether to skip subchannel method handling and send all method calls to the 3D engine
             * @return If the this was the final method in the current GpEntry
             */
            auto processMethod{[&] <bool ThreeDOnly> () -> bool {
                switch (methodHeader.secOp) {
                    case PushBufferMethodHeader::SecOp::IncMethod:
                        return dispatchCalls.operator()<ThreeDOnly, MethodResumeState::State::Inc>();
                    case PushBufferMethodHeader::SecOp::NonIncMethod:
                        return dispatchCalls.operator()<ThreeDOnly, MethodResumeState::State::NonInc>();
                    case PushBufferMethodHeader::SecOp::OneInc:
                        return dispatchCalls.operator()<ThreeDOnly, MethodResumeState::State::OneInc>();
                    case PushBufferMethodHeader::SecOp::ImmdDataMethod:
                        if (methodHeader.Pure()) {
                            if constexpr (ThreeDOnly)
                                channelCtx.maxwell3D->CallMethod(methodHeader.methodAddress, methodHeader.immdData);
                            else
                                SendPure(methodHeader.methodAddress, methodHeader.immdData, methodHeader.methodSubChannel);
                        } else {
                            SendFull(methodHeader.methodAddress, methodHeader.immdData, methodHeader.methodSubChannel, true);
                        }
                        return false;
                    case PushBufferMethodHeader::SecOp::EndPbSegment:
                        return true;
                    default:
                        throw exception("Unsupported pushbuffer method SecOp: {}", static_cast<u8>(methodHeader.secOp));
                }
            }};

            bool hitEnd{[&]() {
                if (methodHeader.methodSubChannel == SubchannelId::ThreeD) { [[likely]]
                    return processMethod.operator()<true>();
                } else {
                    channelCtx.maxwell3D->FlushEngineState(); // Flush the 3D engine state when doing any calls to other engines
                    return processMethod.operator()<false>();
                }
            }()};

            if (hitEnd)
                break;
        }
    }

    void ChannelGpfifo::Run() {
        if (int result{pthread_setname_np(pthread_self(), "GPFIFO")})
            Logger::Warn("Failed to set the thread name: {}", strerror(result));

        try {
            signal::SetSignalHandler({SIGINT, SIGILL, SIGTRAP, SIGBUS, SIGFPE}, signal::ExceptionalSignalHandler);
            signal::SetSignalHandler({SIGSEGV}, nce::NCE::HostSignalHandler); // We may access NCE trapped memory

            gpEntries.Process([this](GpEntry gpEntry) {
                Logger::Debug("Processing pushbuffer: 0x{:X}, Size: 0x{:X}", gpEntry.Address(), +gpEntry.size);
                Process(gpEntry);
            }, [this]() {
                // If we run out of GpEntries to process ensure we submit any remaining GPU work before waiting for more to arrive
                Logger::Debug("Finished processing pushbuffer batch");
                channelCtx.executor.Submit();
            });
        } catch (const signal::SignalException &e) {
            if (e.signal != SIGINT) {
                Logger::Error("{}\nStack Trace:{}", e.what(), state.loader->GetStackTrace(e.frames));
                Logger::EmulationContext.Flush();
                signal::BlockSignal({SIGINT});
                state.process->Kill(false);
            }
        } catch (const exception &e) {
            Logger::ErrorNoPrefix("{}\nStack Trace:{}", e.what(), state.loader->GetStackTrace(e.frames));
            Logger::EmulationContext.Flush();
            signal::BlockSignal({SIGINT});
            state.process->Kill(false);
        } catch (const std::exception &e) {
            Logger::Error(e.what());
            Logger::EmulationContext.Flush();
            signal::BlockSignal({SIGINT});
            state.process->Kill(false);
        }
    }

    void ChannelGpfifo::Push(span<GpEntry> entries) {
        gpEntries.Append(entries);
    }

    void ChannelGpfifo::Push(GpEntry entry) {
        gpEntries.Push(entry);
    }

    ChannelGpfifo::~ChannelGpfifo() {
        if (thread.joinable()) {
            pthread_kill(thread.native_handle(), SIGINT);
            thread.join();
        }
    }
}
