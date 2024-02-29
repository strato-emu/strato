// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"
#include "hle/symbol_hooks.h"

namespace skyline::nce {
    /**
     * @brief The NCE (Native Code Execution) class is responsible for managing state relevant to the layer between the host and guest
     */
    class NCE {
      private:
        const DeviceState &state;

        std::vector<hle::HookedSymbol> hookedSymbols; //!< The list of symbols that are hooked, these have a specific ordering that is hardcoded into the hooked functions

        static void SvcHandler(u16 svcId, ThreadContext *ctx);

        /**
         * @brief A parameter packed into a 64-bit register denoting the hook which is being called
         */
        struct HookId {
            union {
                struct {
                    u64 index : 63;
                    u64 isExit : 1;
                };
                u64 raw;
            };

            constexpr HookId(u64 index, bool isExit) : index{index}, isExit{isExit} {}
        };

        static void HookHandler(HookId hookId, ThreadContext *ctx);

      public:
        /**
         * @brief An exception which causes the throwing thread to exit alongside all threads optionally
         * @note Exiting must not be performed directly as it could leak temporary objects on the stack by not calling their destructors
         */
        struct ExitException : std::exception {
            bool killAllThreads; //!< If to kill all threads or just the throwing thread

            ExitException(bool killAllThreads = true);

            virtual const char *what() const noexcept;
        };

        /**
         * @brief Handles any signals in the NCE threads
         */
        static void SignalHandler(int signal, siginfo *info, ucontext *ctx, void **tls);

        /**
         * @brief Handles signals for any host threads which may access NCE trapped memory
         * @note Any untrapped SIGSEGVs will emit SIGTRAP when a debugger is attached rather than throwing an exception
         */
        static void HostSignalHandler(int signal, siginfo *info, ucontext *ctx);

        /**
         * @note There should only be one instance of NCE concurrently
         */
        NCE(const DeviceState &state);

        struct PatchData {
            size_t size; //!< Size of the .patch section
            std::vector<size_t> offsets; //!< Offsets in .text of instructions that need to be patched
        };

        static PatchData GetPatchData(const std::vector<u8> &text);

        /**
         * @brief Writes the .patch section and mutates the code accordingly
         * @param patch A pointer to the .patch section which should be exactly patchSize in size and located before the .text section
         * @param textOffset The offset of the .text section, this must be page-aligned
         */
        static void PatchCode(std::vector<u8> &text, u32 *patch, size_t patchSize, const std::vector<size_t> &offsets, size_t textOffset = 0);

        static size_t GetHookSectionSize(span<hle::HookedSymbolEntry> entries);

        void WriteHookSection(span<hle::HookedSymbolEntry> entries, span<u32> hookSection);
    };
}
