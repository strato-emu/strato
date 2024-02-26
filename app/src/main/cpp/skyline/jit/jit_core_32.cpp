// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#include <common/trace.h>
#include <kernel/types/KProcess.h>
#include <kernel/svc.h>
#include "jit_core_32.h"

namespace skyline::jit {
    JitCore32::JitCore32(const DeviceState &state, u32 coreId) : state{state}, coreId{coreId}, jit{MakeDynarmicJit()} {}

    Dynarmic::A32::Jit JitCore32::MakeDynarmicJit() {
        Dynarmic::A32::UserConfig config;

        config.callbacks = this;
        config.processor_id = coreId;

        config.enable_cycle_counting = false;

        return Dynarmic::A32::Jit{config};
    }

    void JitCore32::Run() {
        auto haltReason{static_cast<HaltReason>(jit.Run())};
        ClearHalt(haltReason);

        switch (haltReason) {
            case HaltReason::Svc:
                SvcHandler(lastSwi);
                break;

            default:
                LOGE("JIT halted: {}", to_string(haltReason));
                break;
        }
    }

    void JitCore32::HaltExecution(HaltReason hr) {
        jit.HaltExecution(ToDynarmicHaltReason(hr));
    }

    void JitCore32::ClearHalt(HaltReason hr) {
        jit.ClearHalt(ToDynarmicHaltReason(hr));
    }

    void JitCore32::SaveContext(ThreadContext32 &context) {
        context.gpr = jit.Regs();
        context.fpr = jit.ExtRegs();
        context.cpsr = jit.Cpsr();
        context.fpscr = jit.Fpscr();
    }

    void JitCore32::RestoreContext(const ThreadContext32 &context) {
        jit.Regs() = context.gpr;
        jit.ExtRegs() = context.fpr;
        jit.SetCpsr(context.cpsr);
        jit.SetFpscr(context.fpscr);
    }

    kernel::svc::SvcContext JitCore32::MakeSvcContext() {
        kernel::svc::SvcContext ctx{};
        const auto &jitRegs{jit.Regs()};

        for (size_t i = 0; i < ctx.regs.size(); i++)
            ctx.regs[i] = static_cast<u64>(jitRegs[i]);

        return ctx;
    }

    void JitCore32::ApplySvcContext(const kernel::svc::SvcContext &svcCtx) {
        auto &jitRegs{jit.Regs()};

        for (size_t i = 0; i < svcCtx.regs.size(); i++)
            jitRegs[i] = static_cast<u32>(svcCtx.regs[i]);
    }

    void JitCore32::SetThreadPointer(u32 threadPtr) {
        // TODO: implement coprocessor 15
    }

    void JitCore32::SetTlsPointer(u32 tlsPtr) {
        // TODO: implement coprocessor 15
    }

    u32 JitCore32::GetPC() {
        return jit.Regs()[15];
    }

    void JitCore32::SetPC(u32 pc) {
        jit.Regs()[15] = pc;
    }

    u32 JitCore32::GetSP() {
        return jit.Regs()[13];
    }

    void JitCore32::SetSP(u32 sp) {
        jit.Regs()[13] = sp;
    }

    u32 JitCore32::GetRegister(u32 reg) {
        return jit.Regs()[reg];
    }

    void JitCore32::SetRegister(u32 reg, u32 value) {
        jit.Regs()[reg] = value;
    }

    void JitCore32::SvcHandler(u32 swi) {
        auto svc{kernel::svc::SvcTable[swi]};
        if (svc) [[likely]] {
            TRACE_EVENT("kernel", perfetto::StaticString{svc.name});
            auto svcContext = MakeSvcContext();
            (svc.function)(state, svcContext);
            ApplySvcContext(svcContext);
        } else {
            throw exception("Unimplemented SVC 0x{:X}", swi);
        }
    }

    template<typename T>
    __attribute__((__always_inline__)) T ReadUnaligned(u8 *ptr) {
        T value;
        std::memcpy(&value, ptr, sizeof(T));
        return value;
    }

    template<typename T>
    __attribute__((__always_inline__)) void WriteUnaligned(u8 *ptr, T value) {
        std::memcpy(ptr, &value, sizeof(T));
    }

    template<typename T>
    __attribute__((__always_inline__)) T JitCore32::MemoryRead(u32 vaddr) {
        // The number of bits needed to encode the size of T minus 1
        constexpr u32 bits = std::bit_width(sizeof(T)) - 1;
        // Compute the mask to have "bits" number of 1s (e.g. 0b111 for 3 bits)
        constexpr u32 mask{(1 << bits) - 1};

        if ((vaddr & mask) == 0) // Aligned access
            return state.process->memory.base.cast<T>()[vaddr >> bits];
        else
            return ReadUnaligned<T>(state.process->memory.base.data() + vaddr);
    }

    template<typename T>
    __attribute__((__always_inline__)) void JitCore32::MemoryWrite(u32 vaddr, T value) {
        // The number of bits needed to encode the size of T minus 1
        constexpr u32 bits = std::bit_width(sizeof(T)) - 1;
        // Compute the mask to have "bits" number of 1s (e.g. 0b111 for 3 bits)
        constexpr u32 mask{(1 << bits) - 1};

        if ((vaddr & mask) == 0) // Aligned access
            state.process->memory.base.cast<T>()[vaddr >> bits] = value;
        else
            WriteUnaligned<T>(state.process->memory.base.data() + vaddr, value);
    }

    u8 JitCore32::MemoryRead8(u32 vaddr) {
        return MemoryRead<u8>(vaddr);
    }

    u16 JitCore32::MemoryRead16(u32 vaddr) {
        return MemoryRead<u16>(vaddr);
    }

    u32 JitCore32::MemoryRead32(u32 vaddr) {
        return MemoryRead<u32>(vaddr);
    }

    u64 JitCore32::MemoryRead64(u32 vaddr) {
        return MemoryRead<u64>(vaddr);
    }

    void JitCore32::MemoryWrite8(u32 vaddr, u8 value) {
        MemoryWrite<u8>(vaddr, value);
    }

    void JitCore32::MemoryWrite16(u32 vaddr, u16 value) {
        MemoryWrite<u16>(vaddr, value);
    }

    void JitCore32::MemoryWrite32(u32 vaddr, u32 value) {
        MemoryWrite<u32>(vaddr, value);
    }

    void JitCore32::MemoryWrite64(u32 vaddr, u64 value) {
        MemoryWrite<u64>(vaddr, value);
    }

    void JitCore32::InterpreterFallback(u32 pc, size_t numInstructions) {
        // This is never called in practice.
        state.process->Kill(false, true);
    }

    void JitCore32::CallSVC(u32 swi) {
        lastSwi = swi;
        HaltExecution(HaltReason::Svc);
    }

    void JitCore32::ExceptionRaised(u32 pc, Dynarmic::A32::Exception exception) {
        // Do something.
    }
}
