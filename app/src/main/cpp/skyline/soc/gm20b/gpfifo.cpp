// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <common/signal.h>
#include <common/settings.h>
#include <loader/loader.h>
#include <kernel/types/KProcess.h>
#include <soc.h>
#include <os.h>
#include "channel.h"
#include "macro/macro_state.h"

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
            u32 size{[&]() -> u32  {
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

            u32 end{static_cast<u32>(methodAddress + size)};
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

    void ChannelGpfifo::SendFull(u32 method, GpfifoArgument argument, SubchannelId subChannel, bool lastCall) {
        if (method < engine::GPFIFO::RegisterCount) {
            gpfifoEngine.CallMethod(method, *argument);
        } else if (method < engine::EngineMethodsEnd) { [[likely]]
            SendPure(method, *argument, subChannel);
        } else {
            switch (subChannel) {
                case SubchannelId::ThreeD:
                    skipDirtyFlushes = channelCtx.maxwell3D.HandleMacroCall(method - engine::EngineMethodsEnd, argument, lastCall,
                                                                            [&executor = channelCtx.executor] {
                                                                                executor.Submit({}, true);
                                                                            });
                    break;
                case SubchannelId::TwoD:
                    skipDirtyFlushes = channelCtx.fermi2D.HandleMacroCall(method - engine::EngineMethodsEnd, argument, lastCall,
                                                                          [&executor = channelCtx.executor] {
                                                                              executor.Submit({}, true);
                                                                          });
                    break;
                default:
                    Logger::Warn("Called method 0x{:X} out of bounds for engine 0x{:X}, args: 0x{:X}", method, subChannel, *argument);
                    break;
            }
        }
    }

    void ChannelGpfifo::SendPure(u32 method, u32 argument, SubchannelId subChannel) {
        if (subChannel == SubchannelId::ThreeD) [[likely]] {
            channelCtx.maxwell3D.CallMethod(method, argument);
            return;
        }

        switch (subChannel) {
            case SubchannelId::ThreeD:
                channelCtx.maxwell3D.CallMethod(method, argument);
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
                channelCtx.maxwell3D.CallMethodBatchNonInc(method, arguments);
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

        auto pushBufferMappedRanges{channelCtx.asCtx->gmmu.TranslateRange(gpEntry.Address(), gpEntry.size * sizeof(u32))};

        bool pushBufferCopied{}; //!< Set by the below lambda in order to track if the pushbuffer is a copy of guest memory or not
        auto pushBuffer{[&]() -> span<u32> {
            if (pushBufferMappedRanges.size() == 1) {
                return pushBufferMappedRanges.front().cast<u32>();
            } else {
                // Create an intermediate copy of pushbuffer data if it's split across multiple mappings
                pushBufferData.resize(gpEntry.size);
                channelCtx.asCtx->gmmu.Read<u32>(pushBufferData, gpEntry.Address());
                pushBufferCopied = true;
                return span(pushBufferData);
            }
        }()};

        bool pushbufferDirty{false};

        for (auto range : pushBufferMappedRanges) {
            if (channelCtx.executor.usageTracker.dirtyIntervals.Intersect(range)) {
                if (skipDirtyFlushes)
                    pushbufferDirty = true;
                else
                    channelCtx.executor.Submit({}, true);
            }
        }

        // There will be at least one entry here
        auto entry{pushBuffer.begin()};

        auto getArgument{[&](){
            return GpfifoArgument{pushBufferCopied ? *entry : 0, pushBufferCopied ? nullptr : entry.base(), pushbufferDirty};
        }};

        // Executes the current split method, returning once execution is finished or the current GpEntry has reached its end
        auto resumeSplitMethod{[&](){
            switch (resumeState.state) {
                case MethodResumeState::State::Inc:
                    while (entry != pushBuffer.end() && resumeState.remaining) {
                        SendFull(resumeState.address++, getArgument(), resumeState.subChannel, --resumeState.remaining == 0);
                        entry++;
                    }

                    break;
                case MethodResumeState::State::OneInc:
                    SendFull(resumeState.address++, getArgument(), resumeState.subChannel, --resumeState.remaining == 0);
                    entry++;

                    // After the first increment OneInc methods work the same as a NonInc method, this is needed so they can resume correctly if they are broken up by multiple GpEntries
                    resumeState.state = MethodResumeState::State::NonInc;
                    [[fallthrough]];
                case MethodResumeState::State::NonInc:
                    while (entry != pushBuffer.end() && resumeState.remaining) {
                        SendFull(resumeState.address, getArgument(), resumeState.subChannel, --resumeState.remaining == 0);
                        entry++;
                    }

                    break;
            }
        }};

        // We've a method from a previous GpEntry that needs resuming
        if (resumeState.remaining)
            resumeSplitMethod();

        // Process more methods if the entries are still not all used up after handling resuming
        for (; entry != pushBuffer.end(); entry++) {
            if (entry >= pushBuffer.end()) [[unlikely]]
                throw exception("GPFIFO buffer overflow!"); // This should never happen

            // Entries containing all zeroes is a NOP, skip over them
            for (; *entry == 0; entry++)
                if (entry == std::prev(pushBuffer.end()))
                    return;

            PushBufferMethodHeader methodHeader{.raw = *entry};

            // Needed in order to check for methods split across multiple GpEntries
            ssize_t remainingEntries{std::distance(entry, pushBuffer.end()) - 1};

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
             */
            auto dispatchCalls{[&]<MethodResumeState::State State> () {
                /**
                 * @brief Gets the offset to apply to the method address for a given dispatch loop index
                 */
                auto methodOffset{[] (u32 i) -> u32 {
                    if constexpr(State == MethodResumeState::State::Inc)
                        return i;
                    else if constexpr (State == MethodResumeState::State::OneInc)
                        return i ? 1 : 0;
                    else
                        return 0;
                }};

                constexpr u32 BatchCutoff{4}; //!< Cutoff needed to send method calls in a batch which is espcially important for UBO updates. This helps to avoid the extra overhead batching for small packets.
                // TODO: Only batch for specific target methods like UBO updates, since normal dispatch is generally cheaper

                if (remainingEntries >= methodHeader.methodCount) { [[likely]]
                    if (methodHeader.Pure()) [[likely]] {
                        if constexpr (State == MethodResumeState::State::NonInc) {
                            // For pure noninc methods we can send all method calls as a span in one go
                            if (methodHeader.methodCount > BatchCutoff) [[unlikely]] {
                                SendPureBatchNonInc(methodHeader.methodAddress, span(&(*++entry), methodHeader.methodCount), methodHeader.methodSubChannel);

                                entry += methodHeader.methodCount - 1;
                                return false;
                            }
                        } else if constexpr (State == MethodResumeState::State::OneInc) {
                            // For pure oneinc methods we can send the initial method then send the rest as a span in one go
                            if (methodHeader.methodCount > (BatchCutoff + 1)) [[unlikely]] {
                                SendPure(methodHeader.methodAddress, *++entry, methodHeader.methodSubChannel);
                                SendPureBatchNonInc(methodHeader.methodAddress + 1, span((++entry).base(), methodHeader.methodCount - 1), methodHeader.methodSubChannel);

                                entry += methodHeader.methodCount - 2;
                                return false;
                            }
                        }

                        #pragma unroll(2)
                        for (u32 i{}; i < methodHeader.methodCount; i++)
                            SendPure(methodHeader.methodAddress + methodOffset(i), *++entry, methodHeader.methodSubChannel);
                    } else {
                        // Slow path for methods that touch GPFIFO or macros
                        for (u32 i{}; i < methodHeader.methodCount; i++) {
                            entry++;
                            SendFull(methodHeader.methodAddress + methodOffset(i), getArgument(), methodHeader.methodSubChannel, i == methodHeader.methodCount - 1);
                        }
                    }
                } else {
                    startSplitMethod(State);
                    return true;
                }

                return false;
            }};

            /**
             * @brief Handles execution of a single method
             * @return If the this was the final method in the current GpEntry
             */
            auto processMethod{[&] () -> bool {
                if (methodHeader.secOp == PushBufferMethodHeader::SecOp::IncMethod)  [[likely]] {
                    return dispatchCalls.operator()<MethodResumeState::State::Inc>();
                } else if (methodHeader.secOp == PushBufferMethodHeader::SecOp::OneInc) [[likely]] {
                    return dispatchCalls.operator()<MethodResumeState::State::OneInc>();
                } else if (methodHeader.secOp == PushBufferMethodHeader::SecOp::ImmdDataMethod) {
                    if (methodHeader.Pure())
                        SendPure(methodHeader.methodAddress, methodHeader.immdData, methodHeader.methodSubChannel);
                    else
                        SendFull(methodHeader.methodAddress, GpfifoArgument{methodHeader.immdData}, methodHeader.methodSubChannel, true);

                    return false;
                } else if (methodHeader.secOp == PushBufferMethodHeader::SecOp::NonIncMethod) [[unlikely]] {
                    return dispatchCalls.operator()<MethodResumeState::State::NonInc>();
                } else if (methodHeader.secOp == PushBufferMethodHeader::SecOp::EndPbSegment) [[unlikely]] {
                    return true;
                } else if (methodHeader.secOp == PushBufferMethodHeader::SecOp::Grp0UseTert) {
                    if (methodHeader.tertOp == PushBufferMethodHeader::TertOp::Grp0SetSubDevMask)
                        return false;

                    throw exception("Unsupported pushbuffer method TertOp: {}", static_cast<u8>(methodHeader.tertOp));
                } else {
                    throw exception("Unsupported pushbuffer method SecOp: {}", static_cast<u8>(methodHeader.secOp));
                }
            }};

            bool hitEnd{[&]() {
                if (methodHeader.methodSubChannel != SubchannelId::ThreeD) [[unlikely]]
                    channelCtx.maxwell3D.FlushEngineState(); // Flush the 3D engine state when doing any calls to other engines
                return processMethod();
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

            bool channelLocked{};

            gpEntries.Process([this, &channelLocked](GpEntry gpEntry) {
                Logger::Debug("Processing pushbuffer: 0x{:X}, Size: 0x{:X}", gpEntry.Address(), +gpEntry.size);

                if (!channelLocked) {
                    channelCtx.Lock();
                    channelLocked = true;
                }

                Process(gpEntry);
            }, [this, &channelLocked]() {
                // If we run out of GpEntries to process ensure we submit any remaining GPU work before waiting for more to arrive
                Logger::Debug("Finished processing pushbuffer batch");
                if (channelLocked) {
                    channelCtx.executor.Submit();
                    channelCtx.Unlock();
                    channelLocked = false;
                }
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
