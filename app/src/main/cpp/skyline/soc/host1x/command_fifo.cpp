// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/signal.h>
#include <nce.h>
#include <loader/loader.h>
#include <kernel/types/KProcess.h>
#include <soc.h>
#include "command_fifo.h"

namespace skyline::soc::host1x {
    /**
     * @url https://github.com/torvalds/linux/blob/477f70cd2a67904e04c2c2b9bd0fa2e95222f2f6/drivers/gpu/host1x/hw/debug_hw.c#L16
     */
    enum class Host1xOpcode : u8 {
        SetClass = 0x00,
        Incr = 0x01,
        NonIncr = 0x02,
        Mask = 0x03,
        Imm = 0x04,
        Restart = 0x05,
        Gather = 0x06,
        SetStrmId = 0x07,
        SetAppId = 0x08,
        SetPlyd = 0x09,
        IncrW = 0x0a,
        NonIncrW = 0x0b,
        GatherW = 0x0c,
        RestartW = 0x0d,
        Extend = 0x0e,
    };

    union ChannelCommandFifoMethodHeader {
        u32 raw{};
        u16 immdData : 12;
        u16 methodCount;
        u16 offsetMask;

        struct {
            u8 classMethodMask : 6;
            ClassId classId : 10;
            u16 methodAddress : 12;
            Host1xOpcode opcode : 4;
        };
    };
    static_assert(sizeof(ChannelCommandFifoMethodHeader) == sizeof(u32));

    ChannelCommandFifo::ChannelCommandFifo(const DeviceState &state, SyncpointSet &syncpoints) : state(state), gatherQueue(GatherQueueSize), host1XClass(syncpoints), nvDecClass(syncpoints), vicClass(syncpoints) {}

    void ChannelCommandFifo::Send(ClassId targetClass, u32 method, u32 argument) {
        Logger::Verbose("Calling method in class: 0x{:X}, method: 0x{:X}, argument: 0x{:X}", targetClass, method, argument);

        switch (targetClass) {
            case ClassId::Host1x:
                host1XClass.CallMethod(method, argument);
                break;
            case ClassId::NvDec:
                nvDecClass.CallMethod(method, argument);
                break;
            case ClassId::VIC:
                vicClass.CallMethod(method, argument);
                break;
            default:
                Logger::Error("Sending method to unimplemented class: 0x{:X}", targetClass);
                break;
        }
    }

    void ChannelCommandFifo::Process(span<u32> gather) {
        ClassId targetClass{ClassId::Host1x};

        for (auto entry{gather.begin()}; entry != gather.end(); entry++) {
            ChannelCommandFifoMethodHeader methodHeader{.raw = *entry};

            switch (methodHeader.opcode) {
                case Host1xOpcode::SetClass:
                    targetClass = methodHeader.classId;

                    for (u32 i{}; i < std::numeric_limits<u8>::digits; i++)
                        if (methodHeader.classMethodMask & (1 << i))
                            Send(targetClass, methodHeader.methodAddress + i, *++entry);

                    break;
                case Host1xOpcode::Incr:
                    for (u32 i{}; i < methodHeader.methodCount; i++)
                        Send(targetClass, methodHeader.methodAddress + i, *++entry);

                    break;
                case Host1xOpcode::NonIncr:
                    for (u32 i{}; i < methodHeader.methodCount; i++)
                        Send(targetClass, methodHeader.methodAddress, *++entry);

                    break;
                case Host1xOpcode::Mask:
                    for (u32 i{}; i < std::numeric_limits<u16>::digits; i++)
                        if (methodHeader.offsetMask & (1 << i))
                            Send(targetClass, methodHeader.methodAddress + i, *++entry);

                    break;
                case Host1xOpcode::Imm:
                    Send(targetClass, methodHeader.methodAddress, methodHeader.immdData);
                    break;
                default:
                    throw exception("Unimplemented Host1x command FIFO opcode: 0x{:X}", static_cast<u8>(methodHeader.opcode));
            }
        }
    }

    void ChannelCommandFifo::Start() {
        std::scoped_lock lock(threadStartMutex);

        if (!thread.joinable())
            thread = std::thread(&ChannelCommandFifo::Run, this);
    }

    void ChannelCommandFifo::Run() {
        if (int result{pthread_setname_np(pthread_self(), "ChannelCmdFifo")})
            Logger::Warn("Failed to set the thread name: {}", strerror(result));

        try {
            signal::SetSignalHandler({SIGINT, SIGILL, SIGTRAP, SIGBUS, SIGFPE}, signal::ExceptionalSignalHandler);
            signal::SetSignalHandler({SIGSEGV}, nce::NCE::HostSignalHandler); // We may access NCE trapped memory

            gatherQueue.Process([this](span<u32> gather) {
                Logger::Debug("Processing pushbuffer: 0x{:X}, size: 0x{:X}", gather.data(), gather.size());
                Process(gather);
            }, [] {});
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

    void ChannelCommandFifo::Push(span<u32> gather) {
        gatherQueue.Push(gather);
    }

    ChannelCommandFifo::~ChannelCommandFifo() {
        if (thread.joinable()) {
            pthread_kill(thread.native_handle(), SIGINT);
            thread.join();
        }
    }
}
