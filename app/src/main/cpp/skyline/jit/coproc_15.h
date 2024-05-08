// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2024 Strato Team and Contributors (https://github.com/strato-emu/)

#pragma once

#include <optional>
#include <common/base.h>
#include <dynarmic/interface/A32/coprocessor.h>

namespace skyline::jit {
    class Coprocessor15 final : public Dynarmic::A32::Coprocessor {
      public:
        using CoprocReg = Dynarmic::A32::CoprocReg;

        Coprocessor15() = default;

        std::optional<Callback> CompileInternalOperation(bool two, unsigned opc1, CoprocReg CRd, CoprocReg CRn, CoprocReg CRm, unsigned opc2) override;

        CallbackOrAccessOneWord CompileSendOneWord(bool two, unsigned opc1, CoprocReg CRn, CoprocReg CRm, unsigned opc2) override;

        CallbackOrAccessTwoWords CompileSendTwoWords(bool two, unsigned opc, CoprocReg CRm) override;

        CallbackOrAccessOneWord CompileGetOneWord(bool two, unsigned opc1, CoprocReg CRn, CoprocReg CRm, unsigned opc2) override;

        CallbackOrAccessTwoWords CompileGetTwoWords(bool two, unsigned opc, CoprocReg CRm) override;

        std::optional<Callback> CompileLoadWords(bool two, bool long_transfer, CoprocReg CRd, std::optional<u8> option) override;

        std::optional<Callback> CompileStoreWords(bool two, bool long_transfer, CoprocReg CRd, std::optional<u8> option) override;

        u32 tpidrurw = 0; //!< Thread ID Register User and Privileged R/W accessible (equivalent to aarch64 TPIDR_EL0)
        u32 tpidruro = 0; //!< Thread ID Register User read-only and Privileged R/W accessible (equivalent to aarch64 TPIDRRO_EL0)
    };
}
