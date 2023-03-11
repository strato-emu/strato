// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 yuzu Emulator Project (https://yuzu-emu.org/)
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <range/v3/algorithm/any_of.hpp>
#include <soc/gm20b/engines/maxwell/types.h>
#include <soc/gm20b/engines/engine.h>
#include "macro_state.h"

namespace skyline::soc::gm20b {
    static bool AnyArgsDirty(span<GpfifoArgument> args) {
        return ranges::any_of(args, [](const GpfifoArgument &arg) { return arg.dirty; });
    }

    static bool TopologyRequiresConversion(engine::maxwell3d::type::DrawTopology topology) {
        switch (topology) {
            case engine::maxwell3d::type::DrawTopology::Quads:
            case engine::maxwell3d::type::DrawTopology::QuadStrip:
            case engine::maxwell3d::type::DrawTopology::Polygon:
                return true;
            default:
                return false;
        }
    }

    namespace macro_hle {
        bool DrawInstanced(size_t offset, span<GpfifoArgument> args, engine::MacroEngineBase *targetEngine, const std::function<void(void)> &flushCallback) {
            if (AnyArgsDirty(args))
                flushCallback();

            u32 instanceCount{targetEngine->ReadMethodFromMacro(0xD1B) & *args[2]};

            targetEngine->DrawInstanced(*args[0], *args[1], instanceCount, *args[3], *args[4]);
            return true;
        }

        bool DrawInstancedIndexedIndirect(size_t offset, span<GpfifoArgument> args, engine::MacroEngineBase *targetEngine, const std::function<void(void)> &flushCallback) {
            u32 topology{*args[0]};
            bool topologyConversion{TopologyRequiresConversion(static_cast<engine::maxwell3d::type::DrawTopology>(topology))};

            // If the indirect topology isn't supported flush and fallback to a non indirect draw
            if (topologyConversion && args[1].dirty)
                flushCallback();

            if (topologyConversion || !args[1].dirty) {
                u32 instanceCount{targetEngine->ReadMethodFromMacro(0xD1B) & *args[2]};
                targetEngine->DrawIndexedInstanced(topology, *args[1], instanceCount, *args[4], *args[3], *args[5]);
            } else {
                targetEngine->DrawIndexedIndirect(topology, span(args[1].argumentPtr, 5).cast<u8>(), 1, 0);
            }

            return true;
        }

        struct HleFunctionInfo {
            Function function;
            u64 size;
            u32 hash;
        };

        constexpr std::array<HleFunctionInfo, 0x4> functions{{
            {DrawInstanced, 0x12, 0x2FDD711},
            {DrawInstancedIndexedIndirect, 0x17, 0xDBC3B762},
            {DrawInstancedIndexedIndirect, 0x1F, 0xDA07F4E5} // This macro is the same as above but it writes draw params to a cbuf, which are unnecessary due to hades HLE
        }};

        static Function LookupFunction(span<u32> code) {
            for (const auto &function : functions) {
                if (function.size > code.size())
                    continue;

                auto macro{code.subspan(0, function.size)};
                if (XXH32(macro.data(), macro.size_bytes(), 0) == function.hash)
                    return function.function;
            }

            return {};
        }
    }

    void MacroState::Invalidate() {
        invalidatePending = true;
    }

    void MacroState::Execute(u32 position, span<GpfifoArgument> args, engine::MacroEngineBase *targetEngine, const std::function<void(void)> &flushCallback) {
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

        if (macroHleFunctions[position].function && macroHleFunctions[position].function(offset, args, targetEngine, flushCallback))
            return;

        if (AnyArgsDirty(args))
            flushCallback();

        argumentStorage.resize(args.size());
        std::transform(args.begin(), args.end(), argumentStorage.begin(), [](GpfifoArgument arg) { return *arg; });
        macroInterpreter.Execute(offset, argumentStorage, targetEngine);
    }
}
