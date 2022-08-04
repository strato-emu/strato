// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <cxxabi.h>
#include <unistd.h>
#include "common/signal.h"
#include "common/trace.h"
#include "os.h"
#include "jvm.h"
#include "kernel/types/KProcess.h"
#include "kernel/svc.h"
#include "nce/guest.h"
#include "nce/instructions.h"
#include "nce.h"

namespace skyline::nce {
    NCE::ExitException::ExitException(bool killAllThreads) : killAllThreads(killAllThreads) {}

    const char *NCE::ExitException::what() const noexcept {
        return killAllThreads ? "ExitProcess" : "ExitThread";
    }

    void NCE::SvcHandler(u16 svcId, ThreadContext *ctx) {
        TRACE_EVENT_END("guest");

        const auto &state{*ctx->state};
        auto svc{kernel::svc::SvcTable[svcId]};
        try {
            if (svc) [[likely]] {
                TRACE_EVENT("kernel", perfetto::StaticString{svc.name});
                (svc.function)(state);
            } else {
                throw exception("Unimplemented SVC 0x{:X}", svcId);
            }

            while (kernel::Scheduler::YieldPending) [[unlikely]] {
                state.scheduler->Rotate(false);
                kernel::Scheduler::YieldPending = false;
                state.scheduler->WaitSchedule();
            }
        } catch (const signal::SignalException &e) {
            if (e.signal != SIGINT) {
                Logger::ErrorNoPrefix("{} (SVC: {})\nStack Trace:{}", e.what(), svc.name, state.loader->GetStackTrace(e.frames));
                Logger::EmulationContext.Flush();

                if (state.thread->id) {
                    signal::BlockSignal({SIGINT});
                    state.process->Kill(false);
                }
            } else {
                Logger::EmulationContext.Flush();
            }

            abi::__cxa_end_catch(); // We call this prior to the longjmp to cause the exception object to be destroyed
            std::longjmp(state.thread->originalCtx, true);
        } catch (const ExitException &e) {
            if (e.killAllThreads && state.thread->id) {
                signal::BlockSignal({SIGINT});
                state.process->Kill(false);
            }

            abi::__cxa_end_catch();
            std::longjmp(state.thread->originalCtx, true);
        } catch (const exception &e) {
            Logger::ErrorNoPrefix("{}\nStack Trace:{}", e.what(), state.loader->GetStackTrace(e.frames));
            Logger::EmulationContext.Flush();

            if (state.thread->id) {
                signal::BlockSignal({SIGINT});
                state.process->Kill(false);
            }

            abi::__cxa_end_catch();
            std::longjmp(state.thread->originalCtx, true);
        } catch (const std::exception &e) {
            if (svc)
                Logger::ErrorNoPrefix("{} (SVC: {})\nStack Trace:{}", e.what(), svc.name, state.loader->GetStackTrace());
            else
                Logger::ErrorNoPrefix("{} (SVC: 0x{:X})\nStack Trace:{}", e.what(), svcId, state.loader->GetStackTrace());

            Logger::EmulationContext.Flush();

            if (state.thread->id) {
                signal::BlockSignal({SIGINT});
                state.process->Kill(false);
            }

            abi::__cxa_end_catch();
            std::longjmp(state.thread->originalCtx, true);
        }

        TRACE_EVENT_BEGIN("guest", "Guest");
    }

    void NCE::SignalHandler(int signal, siginfo *info, ucontext *ctx, void **tls) {
        if (*tls) { // If TLS was restored then this occurred in guest code
            auto &mctx{ctx->uc_mcontext};
            const auto &state{*reinterpret_cast<ThreadContext *>(*tls)->state};

            if (signal == SIGSEGV)
                // If we get a guest access violation then we want to handle any accesses that may be from a trapped region
                if (state.nce->TrapHandler(reinterpret_cast<u8 *>(info->si_addr), true))
                    return;

            if (signal != SIGINT) {
                signal::StackFrame topFrame{.lr = reinterpret_cast<void *>(ctx->uc_mcontext.pc), .next = reinterpret_cast<signal::StackFrame *>(ctx->uc_mcontext.regs[29])};
                std::string trace{state.loader->GetStackTrace(&topFrame)};

                std::string cpuContext;
                if (mctx.fault_address)
                    cpuContext += fmt::format("\n  Fault Address: 0x{:X}", mctx.fault_address);
                if (mctx.sp)
                    cpuContext += fmt::format("\n  Stack Pointer: 0x{:X}", mctx.sp);
                for (size_t index{}; index < (sizeof(mcontext_t::regs) / sizeof(u64)); index += 2)
                    cpuContext += fmt::format("\n  X{:<2}: 0x{:<16X} X{:<2}: 0x{:X}", index, mctx.regs[index], index + 1, mctx.regs[index + 1]);

                Logger::Error("Thread #{} has crashed due to signal: {}\nStack Trace:{}\nCPU Context:{}", state.thread->id, strsignal(signal), trace, cpuContext);
                Logger::EmulationContext.Flush();

                if (state.thread->id) {
                    signal::BlockSignal({SIGINT});
                    state.process->Kill(false);
                }
            }

            mctx.pc = reinterpret_cast<u64>(&std::longjmp);
            mctx.regs[0] = reinterpret_cast<u64>(state.thread->originalCtx);
            mctx.regs[1] = true;

            *tls = nullptr;
        } else { // If TLS wasn't restored then this occurred in host code
            HostSignalHandler(signal, info, ctx);
        }
    }

    static NCE *staticNce{nullptr}; //!< A static instance of NCE for use in the signal handler

    void NCE::HostSignalHandler(int signal, siginfo *info, ucontext *ctx) {
        if (signal == SIGSEGV) {
            if (staticNce && staticNce->TrapHandler(reinterpret_cast<u8 *>(info->si_addr), true))
                return;

            bool runningUnderDebugger{[]() {
                static std::ifstream status("/proc/self/status");
                status.seekg(0);

                constexpr std::string_view TracerPidTag{"TracerPid:"};
                for (std::string line; std::getline(status, line);) {
                    if (line.starts_with(TracerPidTag)) {
                        line = line.substr(TracerPidTag.size());

                        for (char character : line)
                            if (std::isspace(character))
                                continue;
                            else
                                return character != '0';

                        return false;
                    }
                }

                return false;
            }()};

            if (runningUnderDebugger) {
                /* Variables for debugger, these are meant to be read and utilized by the debugger to break in user code with all registers intact */
                void *pc{reinterpret_cast<void *>(ctx->uc_mcontext.pc)}; // Use 'p pc' to get the value of this and 'breakpoint set -t current -a ${value of pc}' to break in user code
                bool shouldReturn{true}; // Set this to false to throw an exception instead of returning

                raise(SIGTRAP); // Notify the debugger if we've got a SIGSEGV as the debugger doesn't catch them by default as they might be hooked

                if (shouldReturn)
                    return;
            }
        }

        signal::ExceptionalSignalHandler(signal, info, ctx); // Delegate throwing a host exception to the exceptional signal handler
    }

    void *NceTlsRestorer() {
        ThreadContext *threadCtx;
        asm volatile("MRS %x0, TPIDR_EL0":"=r"(threadCtx));
        if (threadCtx->magic != constant::SkyTlsMagic)
            return nullptr;
        asm volatile("MSR TPIDR_EL0, %x0"::"r"(threadCtx->hostTpidrEl0));
        return threadCtx;
    }

    NCE::NCE(const DeviceState &state) : state(state) {
        signal::SetTlsRestorer(&NceTlsRestorer);
        staticNce = this;
    }

    NCE::~NCE() {
        staticNce = nullptr;
    }

    constexpr u8 MainSvcTrampolineSize{17}; // Size of the main SVC trampoline function in u32 units
    constexpr u32 TpidrEl0{0x5E82};         // ID of TPIDR_EL0 in MRS
    constexpr u32 TpidrroEl0{0x5E83};       // ID of TPIDRRO_EL0 in MRS
    constexpr u32 CntfrqEl0{0x5F00};        // ID of CNTFRQ_EL0 in MRS
    constexpr u32 CntpctEl0{0x5F01};        // ID of CNTPCT_EL0 in MRS
    constexpr u32 CntvctEl0{0x5F02};        // ID of CNTVCT_EL0 in MRS
    constexpr u32 TegraX1Freq{19200000};    // The clock frequency of the Tegra X1 (19.2 MHz)

    NCE::PatchData NCE::GetPatchData(const std::vector<u8> &text) {
        size_t size{guest::SaveCtxSize + guest::LoadCtxSize + MainSvcTrampolineSize};
        std::vector<size_t> offsets;

        u64 frequency;
        asm("MRS %0, CNTFRQ_EL0" : "=r"(frequency));
        bool rescaleClock{frequency != TegraX1Freq};

        auto start{reinterpret_cast<const u32 *>(text.data())}, end{reinterpret_cast<const u32 *>(text.data() + text.size())};
        for (const u32 *instruction{start}; instruction < end; instruction++) {
            auto svc{*reinterpret_cast<const instructions::Svc *>(instruction)};
            auto mrs{*reinterpret_cast<const instructions::Mrs *>(instruction)};
            auto msr{*reinterpret_cast<const instructions::Msr *>(instruction)};
            auto instructionOffset{static_cast<size_t>(instruction - start)};

            if (svc.Verify()) {
                size += 7;
                offsets.push_back(instructionOffset);
            } else if (mrs.Verify()) {
                if (mrs.srcReg == TpidrroEl0 || mrs.srcReg == TpidrEl0) {
                    size += ((mrs.destReg != registers::X0) ? 6 : 3);
                    offsets.push_back(instructionOffset);
                } else {
                    if (rescaleClock) {
                        if (mrs.srcReg == CntpctEl0) {
                            size += guest::RescaleClockSize + 3;
                            offsets.push_back(instructionOffset);
                        } else if (mrs.srcReg == CntfrqEl0) {
                            size += 3;
                            offsets.push_back(instructionOffset);
                        }
                    } else if (mrs.srcReg == CntpctEl0) {
                        offsets.push_back(instructionOffset);
                    }
                }
            } else if (msr.Verify() && msr.destReg == TpidrEl0) {
                size += 6;
                offsets.push_back(instructionOffset);
            }
        }
        return {util::AlignUp(size * sizeof(u32), PAGE_SIZE), offsets};
    }

    void NCE::PatchCode(std::vector<u8> &text, u32 *patch, size_t patchSize, const std::vector<size_t> &offsets) {
        u32 *start{patch};
        u32 *end{patch + (patchSize / sizeof(u32))};

        std::memcpy(patch, reinterpret_cast<void *>(&guest::SaveCtx), guest::SaveCtxSize * sizeof(u32));
        patch += guest::SaveCtxSize;

        {
            /* Main SVC Trampoline */
            /* Store LR in 16B of pre-allocated stack */
            *patch++ = 0xF90007FE; // STR LR, [SP, #8]

            /* Replace Skyline TLS with host TLS */
            *patch++ = 0xD53BD041; // MRS X1, TPIDR_EL0
            *patch++ = 0xF9415022; // LDR X2, [X1, #0x2A0] (ThreadContext::hostTpidrEl0)
            *patch++ = 0xD51BD042; // MSR TPIDR_EL0, X2

            /* Replace guest stack with host stack */
            *patch++ = 0x910003E2; // MOV X2, SP
            *patch++ = 0xF9415423; // LDR X3, [X1, #0x2A8] (ThreadContext::hostSp)
            *patch++ = 0x9100007F; // MOV SP, X3

            /* Store Skyline TLS + guest SP on stack */
            *patch++ = 0xA9BF0BE1; // STP X1, X2, [SP, #-16]!

            /* Jump to SvcHandler */
            for (const auto &mov : instructions::MoveRegister(registers::X2, reinterpret_cast<u64>(&NCE::SvcHandler)))
                if (mov)
                    *patch++ = mov;
            *patch++ = 0xD63F0040; // BLR X2

            /* Restore Skyline TLS + guest SP */
            *patch++ = 0xA8C10BE1; // LDP X1, X2, [SP], #16
            *patch++ = 0xD51BD041; // MSR TPIDR_EL0, X1
            *patch++ = 0x9100005F; // MOV SP, X2

            /* Restore LR and Return */
            *patch++ = 0xF94007FE; // LDR LR, [SP, #8]
            *patch++ = 0xD65F03C0; // RET
        }

        std::memcpy(patch, reinterpret_cast<void *>(&guest::LoadCtx), guest::LoadCtxSize * sizeof(u32));
        patch += guest::LoadCtxSize;

        u64 frequency;
        asm("MRS %0, CNTFRQ_EL0" : "=r"(frequency));
        bool rescaleClock{frequency != TegraX1Freq};

        for (auto offset : offsets) {
            u32 *instruction{reinterpret_cast<u32 *>(text.data()) + offset};
            auto svc{*reinterpret_cast<instructions::Svc *>(instruction)};
            auto mrs{*reinterpret_cast<instructions::Mrs *>(instruction)};
            auto msr{*reinterpret_cast<instructions::Msr *>(instruction)};
            auto endOffset{[&] { return static_cast<size_t>(end - patch); }};
            auto startOffset{[&] { return static_cast<size_t>(start - patch); }};

            if (svc.Verify()) {
                /* Per-SVC Trampoline */
                /* Rewrite SVC with B to trampoline */
                *instruction = instructions::B(static_cast<i32>(endOffset() + offset), true).raw;

                /* Save Context */
                *patch++ = 0xF81F0FFE; // STR LR, [SP, #-16]!
                *patch = instructions::BL(static_cast<i32>(startOffset())).raw;
                patch++;

                /* Jump to main SVC trampoline */
                *patch++ = instructions::Movz(registers::W0, static_cast<u16>(svc.value)).raw;
                *patch = instructions::BL(static_cast<i32>(startOffset() + guest::SaveCtxSize)).raw;
                patch++;

                /* Restore Context and Return */
                *patch = instructions::BL(static_cast<i32>(startOffset() + guest::SaveCtxSize + MainSvcTrampolineSize)).raw;
                patch++;
                *patch++ = 0xF84107FE; // LDR LR, [SP], #16
                *patch = instructions::B(static_cast<i32>(endOffset() + offset + 1)).raw;
                patch++;
            } else if (mrs.Verify()) {
                if (mrs.srcReg == TpidrroEl0 || mrs.srcReg == TpidrEl0) {
                    /* Emulated TLS Register Load */
                    /* Rewrite MRS with B to trampoline */
                    *instruction = instructions::B(static_cast<i32>(endOffset() + offset), true).raw;

                    /* Allocate Scratch Register */
                    if (mrs.destReg != registers::X0)
                        *patch++ = 0xF81F0FE0; // STR X0, [SP, #-16]!

                    /* Retrieve emulated TLS register from ThreadContext */
                    *patch++ = 0xD53BD040; // MRS X0, TPIDR_EL0
                    if (mrs.srcReg == TpidrroEl0)
                        *patch++ = 0xF9415800; // LDR X0, [X0, #0x2B0] (ThreadContext::tpidrroEl0)
                    else
                        *patch++ = 0xF9415C00; // LDR X0, [X0, #0x2B8] (ThreadContext::tpidrEl0)

                    /* Restore Scratch Register and Return */
                    if (mrs.destReg != registers::X0) {
                        *patch++ = instructions::Mov(registers::X(mrs.destReg), registers::X0).raw;
                        *patch++ = 0xF84107E0; // LDR X0, [SP], #16
                    }
                    *patch = instructions::B(static_cast<i32>(endOffset() + offset + 1)).raw;
                    patch++;
                } else {
                    if (rescaleClock) {
                        if (mrs.srcReg == CntpctEl0) {
                            /* Physical Counter Load Emulation (With Rescaling) */
                            /* Rewrite MRS with B to trampoline */
                            *instruction = instructions::B(static_cast<i32>(endOffset() + offset), true).raw;

                            /* Rescale host clock */
                            std::memcpy(patch, reinterpret_cast<void *>(&guest::RescaleClock), guest::RescaleClockSize * sizeof(u32));
                            patch += guest::RescaleClockSize;

                            /* Load result from stack into destination register */
                            instructions::Ldr ldr(0xF94003E0); // LDR XOUT, [SP]
                            ldr.destReg = mrs.destReg;
                            *patch++ = ldr.raw;

                            /* Free 32B stack allocation by RescaleClock and Return */
                            *patch++ = {0x910083FF}; // ADD SP, SP, #32
                            *patch = instructions::B(static_cast<i32>(endOffset() + offset + 1)).raw;
                            patch++;
                        } else if (mrs.srcReg == CntfrqEl0) {
                            /* Physical Counter Frequency Load Emulation */
                            /* Rewrite MRS with B to trampoline */
                            *instruction = instructions::B(static_cast<i32>(endOffset() + offset), true).raw;

                            /* Write back Tegra X1 Counter Frequency and Return */
                            for (const auto &mov : instructions::MoveRegister(registers::X(mrs.destReg), TegraX1Freq))
                                *patch++ = mov;
                            *patch = instructions::B(static_cast<i32>(endOffset() + offset + 1)).raw;
                            patch++;
                        }
                    } else if (mrs.srcReg == CntpctEl0) {
                        /* Physical Counter Load Emulation (Without Rescaling) */
                        // We just convert CNTPCT_EL0 -> CNTVCT_EL0 as Linux doesn't allow access to the physical counter
                        *instruction = instructions::Mrs(CntvctEl0, registers::X(mrs.destReg)).raw;
                    }
                }
            } else if (msr.Verify() && msr.destReg == TpidrEl0) {
                /* Emulated TLS Register Store */
                /* Rewrite MSR with B to trampoline */
                *instruction = instructions::B(static_cast<i32>(endOffset() + offset), true).raw;

                /* Allocate Scratch Registers */
                bool x0x1{mrs.srcReg != registers::X0 && mrs.srcReg != registers::X1};
                *patch++ = x0x1 ? 0xA9BF07E0 : 0xA9BF0FE2; // STP X(0/2), X(1/3), [SP, #-16]!

                /* Store new TLS value into ThreadContext */
                *patch++ = x0x1 ? 0xD53BD040 : 0xD53BD042; // MRS X(0/2), TPIDR_EL0
                *patch++ = instructions::Mov(x0x1 ? registers::X1 : registers::X3, registers::X(msr.srcReg)).raw;
                *patch++ = x0x1 ? 0xF9015C01 : 0xF9015C03; // STR X(1/3), [X0, #0x4B8] (ThreadContext::tpidrEl0)

                /* Restore Scratch Registers and Return */
                *patch++ = x0x1 ? 0xA8C107E0 : 0xA8C10FE2; // LDP X(0/2), X(1/3), [SP], #16
                *patch = instructions::B(static_cast<i32>(endOffset() + offset + 1)).raw;
                patch++;
            }
        }
    }

    NCE::CallbackEntry::CallbackEntry(TrapProtection protection, LockCallback lockCallback, TrapCallback readCallback, TrapCallback writeCallback) : protection{protection}, lockCallback{std::move(lockCallback)}, readCallback{std::move(readCallback)}, writeCallback{std::move(writeCallback)} {}

    void NCE::ReprotectIntervals(const std::vector<TrapMap::Interval> &intervals, TrapProtection protection) {
        TRACE_EVENT("host", "NCE::ReprotectIntervals");

        auto reprotectIntervalsWithFunction = [&intervals](auto getProtection) {
            for (auto region : intervals) {
                region = region.Align(PAGE_SIZE);
                mprotect(region.start, region.Size(), getProtection(region));
            }
        };

        // We need to determine the lowest protection possible for the given interval
        if (protection == TrapProtection::None) {
            reprotectIntervalsWithFunction([&](auto region) {
                auto entries{trapMap.GetRange(region)};

                TrapProtection lowestProtection{TrapProtection::None};
                for (const auto &entry : entries) {
                    auto entryProtection{entry.get().protection};
                    if (entryProtection > lowestProtection) {
                        lowestProtection = entryProtection;
                        if (entryProtection == TrapProtection::ReadWrite)
                            return PROT_NONE;
                    }
                }

                switch (lowestProtection) {
                    case TrapProtection::None:
                        return PROT_READ | PROT_WRITE | PROT_EXEC;
                    case TrapProtection::WriteOnly:
                        return PROT_READ | PROT_EXEC;
                    case TrapProtection::ReadWrite:
                        return PROT_NONE;
                }
            });
        } else if (protection == TrapProtection::WriteOnly) {
            reprotectIntervalsWithFunction([&](auto region) {
                auto entries{trapMap.GetRange(region)};
                for (const auto &entry : entries)
                    if (entry.get().protection == TrapProtection::ReadWrite)
                        return PROT_NONE;

                return PROT_READ | PROT_EXEC;
            });
        } else if (protection == TrapProtection::ReadWrite) {
            reprotectIntervalsWithFunction([&](auto region) {
                return PROT_NONE; // No checks are needed as this is already the highest level of protection
            });
        }
    }

    bool NCE::TrapHandler(u8 *address, bool write) {
        TRACE_EVENT("host", "NCE::TrapHandler");

        LockCallback lockCallback{};
        while (true) {
            if (lockCallback) {
                // We want to avoid a deadlock of holding trapMutex while locking the resource inside a callback while another thread holding the resource's mutex waits on trapMutex, we solve this by quitting the loop if a callback would be blocking and attempt to lock the resource externally
                lockCallback();
                lockCallback = {};
            }

            std::scoped_lock lock(trapMutex);

            // Retrieve any callbacks for the page that was faulted
            auto[entries, intervals]{trapMap.GetAlignedRecursiveRange<PAGE_SIZE>(address)};
            if (entries.empty())
                return false; // There's no callbacks associated with this page

            // Do callbacks for every entry in the intervals
            if (write) {
                for (auto entryRef : entries) {
                    auto &entry{entryRef.get()};
                    if (entry.protection == TrapProtection::None)
                        // We don't need to do the callback if the entry doesn't require any protection already
                        continue;

                    if (!entry.writeCallback()) {
                        lockCallback = entry.lockCallback;
                        break;
                    }
                    entry.protection = TrapProtection::None; // We don't need to protect this entry anymore
                }
                if (lockCallback)
                    continue; // We need to retry the loop because a callback was blocking
            } else {
                bool allNone{true}; // If all entries require no protection, we can protect to allow all accesses
                for (auto entryRef : entries) {
                    auto &entry{entryRef.get()};
                    if (entry.protection < TrapProtection::ReadWrite) {
                        // We don't need to do the callback if the entry can already handle read accesses
                        allNone = allNone && entry.protection == TrapProtection::None;
                        continue;
                    }

                    if (!entry.readCallback()) {
                        lockCallback = entry.lockCallback;
                        break;
                    }
                    entry.protection = TrapProtection::WriteOnly; // We only need to trap writes to this entry
                }
                if (lockCallback)
                    continue; // We need to retry the loop because a callback was blocking
                write = allNone;
            }

            int permission{PROT_READ | (write ? PROT_WRITE : 0) | PROT_EXEC};
            for (const auto &interval : intervals)
                // Reprotect the interval to the lowest protection level that the callbacks performed allow
                mprotect(interval.start, interval.Size(), permission);

            return true;
        }
    }

    constexpr NCE::TrapHandle::TrapHandle(const TrapMap::GroupHandle &handle) : TrapMap::GroupHandle(handle) {}

    NCE::TrapHandle NCE::CreateTrap(span<span<u8>> regions, const LockCallback &lockCallback, const TrapCallback &readCallback, const TrapCallback &writeCallback) {
        TRACE_EVENT("host", "NCE::CreateTrap");
        std::scoped_lock lock{trapMutex};
        TrapHandle handle{trapMap.Insert(regions, CallbackEntry{TrapProtection::None, lockCallback, readCallback, writeCallback})};
        return handle;
    }

    void NCE::TrapRegions(TrapHandle handle, bool writeOnly) {
        TRACE_EVENT("host", "NCE::TrapRegions");
        std::scoped_lock lock{trapMutex};
        auto protection{writeOnly ? TrapProtection::WriteOnly : TrapProtection::ReadWrite};
        handle->value.protection = protection;
        ReprotectIntervals(handle->intervals, protection);
    }

    void NCE::PageOutRegions(TrapHandle handle) {
        TRACE_EVENT("host", "NCE::PageOutRegions");
        std::scoped_lock lock{trapMutex};
        for (auto region : handle->intervals) {
            auto freeStart{util::AlignUp(region.start, PAGE_SIZE)}, freeEnd{util::AlignDown(region.end, PAGE_SIZE)}; // We want to avoid the first and last page as they may contain unrelated data
            ssize_t freeSize{freeEnd - freeStart};

            constexpr ssize_t MinimumPageoutSize{PAGE_SIZE}; //!< The minimum size to page out, we don't want to page out small intervals for performance reasons
            if (freeSize > MinimumPageoutSize)
                state.process->memory.FreeMemory(span<u8>{freeStart, static_cast<size_t>(freeSize)});
        }
    }

    void NCE::RemoveTrap(TrapHandle handle) {
        TRACE_EVENT("host", "NCE::RemoveTrap");
        std::scoped_lock lock{trapMutex};
        handle->value.protection = TrapProtection::None;
        ReprotectIntervals(handle->intervals, TrapProtection::None);
    }

    void NCE::DeleteTrap(TrapHandle handle) {
        TRACE_EVENT("host", "NCE::DeleteTrap");
        std::scoped_lock lock{trapMutex};
        handle->value.protection = TrapProtection::None;
        ReprotectIntervals(handle->intervals, TrapProtection::None);
        trapMap.Remove(handle);
    }
}
