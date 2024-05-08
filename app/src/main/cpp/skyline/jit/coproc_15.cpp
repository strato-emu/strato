// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2024 Strato Team and Contributors (https://github.com/strato-emu/)
// Copyright © 2017 Citra Emulator Project

#include "coproc_15.h"
#include <logger/logger.h>
#include <common/utils.h>

template<>
struct fmt::formatter<Dynarmic::A32::CoprocReg> {
    constexpr auto parse(format_parse_context &ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const Dynarmic::A32::CoprocReg &reg, FormatContext &ctx) {
        return fmt::format_to(ctx.out(), "cp{}", static_cast<size_t>(reg));
    }
};

namespace skyline::jit {
    using Callback = Dynarmic::A32::Coprocessor::Callback;
    using CallbackOrAccessOneWord = Dynarmic::A32::Coprocessor::CallbackOrAccessOneWord;
    using CallbackOrAccessTwoWords = Dynarmic::A32::Coprocessor::CallbackOrAccessTwoWords;

    static u32 dummy_value;

    std::optional<Callback> Coprocessor15::CompileInternalOperation(bool two, unsigned opc1, CoprocReg CRd, CoprocReg CRn, CoprocReg CRm, unsigned opc2) {
        LOGE("CP15: cdp{} p15, {}, {}, {}, {}, {}", two ? "2" : "", opc1, CRd, CRn, CRm, opc2);
        return std::nullopt;
    }

    CallbackOrAccessOneWord Coprocessor15::CompileSendOneWord(bool two, unsigned opc1, CoprocReg CRn, CoprocReg CRm, unsigned opc2) {
        if (!two && CRn == CoprocReg::C7 && opc1 == 0 && CRm == CoprocReg::C5 && opc2 == 4) {
            // CP15_FLUSH_PREFETCH_BUFFER
            // This is a dummy write, we ignore the value written here
            return &dummy_value;
        }

        if (!two && CRn == CoprocReg::C7 && opc1 == 0 && CRm == CoprocReg::C10) {
            switch (opc2) {
                case 4:
                    // CP15_DATA_SYNC_BARRIER
                    return Callback{
                        [](void *, std::uint32_t, std::uint32_t) -> std::uint64_t {
                            asm volatile("dsb sy\n\t" : : : "memory");
                            return 0;
                        },
                        std::nullopt,
                    };
                case 5:
                    // CP15_DATA_MEMORY_BARRIER
                    return Callback{
                        [](void *, std::uint32_t, std::uint32_t) -> std::uint64_t {
                            asm volatile("dmb sy\n\t" : : : "memory");
                            return 0;
                        },
                        std::nullopt,
                    };
                default:
                    break;
            }
        }

        if (!two && CRn == CoprocReg::C13 && opc1 == 0 && CRm == CoprocReg::C0 && opc2 == 2) {
            // CP15_THREAD_URW
            return &tpidrurw;
        }

        LOGE("CP15: mcr{} p15, {}, <Rt>, {}, {}, {}", two ? "2" : "", opc1, CRn, CRm, opc2);
        return {};
    }

    CallbackOrAccessTwoWords Coprocessor15::CompileSendTwoWords(bool two, unsigned opc, CoprocReg CRm) {
        LOGE("CP15: mcrr{} p15, {}, <Rt>, <Rt2>, {}", two ? "2" : "", opc, CRm);
        return {};
    }

    CallbackOrAccessOneWord Coprocessor15::CompileGetOneWord(bool two, unsigned opc1, CoprocReg CRn, CoprocReg CRm, unsigned opc2) {
        if (!two && CRn == CoprocReg::C13 && opc1 == 0 && CRm == CoprocReg::C0) {
            switch (opc2) {
                case 2:
                    // CP15_THREAD_URW
                    return &tpidrurw;
                case 3:
                    // CP15_THREAD_URO
                    return &tpidruro;
                default:
                    break;
            }
        }

        LOGE("CP15: mrc{} p15, {}, <Rt>, {}, {}, {}", two ? "2" : "", opc1, CRn, CRm, opc2);
        return {};
    }

    CallbackOrAccessTwoWords Coprocessor15::CompileGetTwoWords(bool two, unsigned opc, CoprocReg CRm) {
        if (!two && opc == 0 && CRm == CoprocReg::C14) {
            // CNTPCT
            return Callback{[](void *arg, u32, u32) -> u64 {
                return util::GetTimeTicks();
            }, std::nullopt};
        }

        LOGE("CP15: mrrc{} p15, {}, <Rt>, <Rt2>, {}", two ? "2" : "", opc, CRm);
        return {};
    }

    std::optional<Callback> Coprocessor15::CompileLoadWords(bool two, bool long_transfer, CoprocReg CRd, std::optional<u8> option) {
        if (option)
            LOGE("CP15: mrrc{}{} p15, {}, [...], {}", two ? "2" : "", long_transfer ? "l" : "", CRd, *option);
        else
            LOGE("CP15: mrrc{}{} p15, {}, [...]", two ? "2" : "", long_transfer ? "l" : "", CRd);

        return std::nullopt;
    }

    std::optional<Callback> Coprocessor15::CompileStoreWords(bool two, bool long_transfer, CoprocReg CRd, std::optional<u8> option) {
        if (option)
            LOGE("CP15: mrrc{}{} p15, {}, [...], {}", two ? "2" : "", long_transfer ? "l" : "", CRd, *option);
        else
            LOGE("CP15: mrrc{}{} p15, {}, [...]", two ? "2" : "", long_transfer ? "l" : "", CRd);

        return std::nullopt;
    }
}
