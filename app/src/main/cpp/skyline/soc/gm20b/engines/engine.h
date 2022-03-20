// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <soc/gm20b/macro/macro_state.h>

namespace skyline::soc::gm20b {
    #define U32_OFFSET(regs, field) (offsetof(regs, field) / sizeof(u32))

    namespace engine {
        /**
         * @brief A 40-bit GMMU virtual address with register-packing
         * @note The registers pack the address with big-endian ordering (but with 32 bit words)
         */
        struct Address {
            u32 high;
            u32 low;

            operator u64() {
                return (static_cast<u64>(high) << 32) | low;
            }
        };
        static_assert(sizeof(Address) == sizeof(u64));

        constexpr u32 EngineMethodsEnd = 0xE00; //!< All methods above this are passed to the MME on supported engines

        /**
         * @brief The MacroEngineBase interface provides an interface that can be used by engines to allow interfacing with the macro executer
         */
        struct MacroEngineBase {
            MacroState &macroState;

            struct {
                size_t index{std::numeric_limits<size_t>::max()};
                std::vector<u32> arguments;

                bool Valid() {
                    return index != std::numeric_limits<size_t>::max();
                }

                void Reset() {
                    index = std::numeric_limits<size_t>::max();
                    arguments.clear();
                }
            } macroInvocation{}; //!< Data for a macro that is pending execution

            MacroEngineBase(MacroState &macroState);

            virtual ~MacroEngineBase() = default;

            /**
             * @brief Calls an engine method with the given parameters
             */
            virtual void CallMethodFromMacro(u32 method, u32 argument) = 0;

            /**
             * @brief Reads the current value for the supplied method
             */
            virtual u32 ReadMethodFromMacro(u32 method) = 0;

            /**
             * @brief Handles a call to a method in the MME space
             * @param macroMethodOffset The target offset from EngineMethodsEnd
             */
            void HandleMacroCall(u32 macroMethodOffset, u32 value, bool lastCall);
        };
    }
}
