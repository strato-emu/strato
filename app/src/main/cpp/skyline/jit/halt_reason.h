// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#pragma once

#include <string>
#include <type_traits>
#include <dynarmic/interface/halt_reason.h>

namespace skyline::jit {
    namespace {
        using DynarmicHaltReasonType = std::underlying_type_t<Dynarmic::HaltReason>;
    }

    /**
     * @brief The reason that the JIT has halted
     * @note The binary representation of this enum's values must match Dynarmic::HaltReason one
     */
    enum class HaltReason : DynarmicHaltReasonType {
        Step = static_cast<DynarmicHaltReasonType>(Dynarmic::HaltReason::Step),
        CacheInvalidation = static_cast<DynarmicHaltReasonType>(Dynarmic::HaltReason::CacheInvalidation),
        MemoryAbort = static_cast<DynarmicHaltReasonType>(Dynarmic::HaltReason::MemoryAbort),
        Svc = static_cast<DynarmicHaltReasonType>(Dynarmic::HaltReason::UserDefined1),
        Preempted = static_cast<DynarmicHaltReasonType>(Dynarmic::HaltReason::UserDefined2)
    };

    inline std::string to_string(HaltReason hr) {
        #define CASE(x) case HaltReason::x: return #x

        switch (hr) {
            CASE(Step);
            CASE(CacheInvalidation);
            CASE(MemoryAbort);
            CASE(Svc);
            CASE(Preempted);
            default:
                return "Unknown";
        }

        #undef CASE
    }

    inline std::string to_string(Dynarmic::HaltReason dhr) {
        return to_string(static_cast<HaltReason>(dhr));
    }

    /**
     * @brief Converts a HaltReason to a Dynarmic::HaltReason
     */
    inline Dynarmic::HaltReason ToDynarmicHaltReason(HaltReason hr) {
        return static_cast<Dynarmic::HaltReason>(hr);
    }
}
