// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 yuzu Emulator Project (https://yuzu-emu.org/)
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <soc/gm20b/engines/engine.h>
#include "macro_state.h"

namespace skyline::soc::gm20b {
    namespace macro_hle {
        void DrawInstanced(size_t offset, span<u32> args, engine::MacroEngineBase *targetEngine) {
            u32 instanceCount{targetEngine->ReadMethodFromMacro(0xD1B) & args[2]};

            targetEngine->DrawInstanced(true, args[0], args[1], instanceCount, args[3], args[4]);
        }

        void DrawIndexedInstanced(size_t offset, span<u32> args, engine::MacroEngineBase *targetEngine) {
            u32 instanceCount{targetEngine->ReadMethodFromMacro(0xD1B) & args[2]};

            targetEngine->DrawIndexedInstanced(true, args[0], args[1], instanceCount, args[3], args[4], args[5]);
        }

        void DrawInstancedIndexedWithConstantBuffer(size_t offset, span<u32> args, engine::MacroEngineBase *targetEngine) {
            // Writes globalBaseVertexIndex and globalBaseInstanceIndex to the bound constant buffer before performing a standard instanced indexed draw
            u32 instanceCount{targetEngine->ReadMethodFromMacro(0xD1B) & args[2]};
            targetEngine->CallMethodFromMacro(0x8e3, 0x640);
            targetEngine->CallMethodFromMacro(0x8e4, args[4]);
            targetEngine->CallMethodFromMacro(0x8e5, args[5]);
            targetEngine->DrawIndexedInstanced(false, args[0], args[1], instanceCount, args[4], args[3], args[5]);
            targetEngine->CallMethodFromMacro(0x8e3, 0x640);
            targetEngine->CallMethodFromMacro(0x8e4, 0x0);
            targetEngine->CallMethodFromMacro(0x8e5, 0x0);
        }

        struct HleFunctionInfo {
            Function function;
            u64 size;
            u32 hash;
        };

        constexpr std::array<HleFunctionInfo, 0x3> functions{{
            {DrawInstanced, 0x12, 0x6F0DD310},
            {DrawIndexedInstanced, 0x17, 0x2764C4F},
            {DrawInstancedIndexedWithConstantBuffer, 0x1F, 0xF2F16988},
        }};

        static Function LookupFunction(span<u32> code) {
            for (const auto &function : functions) {
                if (function.size > code.size())
                    continue;

                auto macro{code.subspan(0, function.size)};

                if (XXH32(code.data(), code.size_bytes(), 0) == function.hash)
                    return function.function;
            }

            return {};
        }
    }

    void MacroState::Invalidate() {
        invalidatePending = true;
    }

    void MacroState::Execute(u32 position, span<u32> args, engine::MacroEngineBase *targetEngine) {
        size_t offset{macroPositions[position]};

        if (invalidatePending) {
            macroHleFunctions.fill({});
            invalidatePending = false;
        }

        auto &hleEntry{macroHleFunctions[position]};

        if (!hleEntry.valid) {
            hleEntry.function = macro_hle::LookupFunction(span(macroCode).subspan(offset));
            hleEntry.valid = true;
        }

        if (macroHleFunctions[position].function)
            macroHleFunctions[position].function(offset, args, targetEngine);
        else
            macroInterpreter.Execute(offset, args, targetEngine);
    }
}
