// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include "macro_interpreter.h"

namespace skyline::soc::gm20b {
    /**
     * @brief A GPFIFO argument that can be either a value or a pointer to a value
     */
    struct GpfifoArgument {
        u32 argument;
        u32 *argumentPtr{};
        bool dirty{};

        GpfifoArgument(u32 argument, u32 *argumentPtr, bool dirty) : argument{argument}, argumentPtr{argumentPtr}, dirty{dirty} {}

        explicit GpfifoArgument(u32 argument) : argument{argument} {}

        u32 operator*() const {
            return argumentPtr ? *argumentPtr : argument;
        }
    };

    namespace macro_hle {
        using Function = bool (*)(size_t offset, span<GpfifoArgument> args, engine::MacroEngineBase *targetEngine, const std::function<void(void)> &flushCallback);
    }

    /**
     * @brief Holds per-channel macro state
     */
    struct MacroState {
        struct MacroHleEntry {
            macro_hle::Function function;
            bool valid;
        };

        engine::MacroInterpreter macroInterpreter; //!< The macro interpreter for handling 3D/2D macros
        std::array<u32, 0x2000> macroCode{}; //!< Stores GPU macros, writes to it will wraparound on overflow
        std::array<size_t, 0x80> macroPositions{}; //!< The positions of each individual macro in macro code memory, there can be a maximum of 0x80 macros at any one time
        std::array<MacroHleEntry, 0x80> macroHleFunctions{}; //!< The HLE functions for each macro position, used to optionally override the interpreter
        std::vector<u32> argumentStorage; //!< Storage for the macro arguments during execution using the interpreter

        bool invalidatePending{};

        MacroState() : macroInterpreter{macroCode} {}

        /**
         * @brief Invalidates the HLE function cache
         */
        void Invalidate();

        /**
         * @brief Executes a macro at a given position, this can either be a HLE function or the interpreter
         */
        void Execute(u32 position, span<GpfifoArgument> args, engine::MacroEngineBase *targetEngine, const std::function<void(void)> &flushCallback);
    };
}
