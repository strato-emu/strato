// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <sys/wait.h>
#include <linux/elf.h>
#include "common.h"
#include "hle/symbol_hooks.h"
#include "common/interval_map.h"

namespace skyline::nce {
    /**
     * @brief The NCE (Native Code Execution) class is responsible for managing state relevant to the layer between the host and guest
     */
    class NCE {
      private:
        const DeviceState &state;

        std::vector<hle::HookedSymbol> hookedSymbols; //!< The list of symbols that are hooked, these have a specific ordering that is hardcoded into the hooked functions

        /**
         * @brief The level of protection that is required for a callback entry
         */
        enum class TrapProtection {
            None = 0, //!< No protection is required
            WriteOnly = 1, //!< Only write protection is required
            ReadWrite = 2, //!< Both read and write protection are required
        };

        using TrapCallback = std::function<bool()>;
        using LockCallback = std::function<void()>;

        struct CallbackEntry {
            TrapProtection protection; //!< The least restrictive protection that this callback needs to have
            LockCallback lockCallback;
            TrapCallback readCallback, writeCallback;

            CallbackEntry(TrapProtection protection, LockCallback lockCallback, TrapCallback readCallback, TrapCallback writeCallback);
        };

        std::mutex trapMutex; //!< Synchronizes the accesses to the trap map
        using TrapMap = IntervalMap<u8*, CallbackEntry>;
        TrapMap trapMap; //!< A map of all intervals and corresponding callbacks that have been registered

        /**
         * @brief Reprotects the intervals to the least restrictive protection given the supplied protection
         */
        void ReprotectIntervals(const std::vector<TrapMap::Interval>& intervals, TrapProtection protection);

        bool TrapHandler(u8* address, bool write);

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

        ~NCE();

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

        struct HookedSymbolEntry : hle::HookedSymbol {
            Elf64_Addr* offset{}; //!< A pointer to the hooked function's offset (st_value) in the ELF's dynsym, this is set by the loader and is used to resolve/update the address of the function

            HookedSymbolEntry(std::string name, const hle::HookType &hook, Elf64_Addr* offset) : HookedSymbol{std::move(name), hook}, offset{offset} {}
        };

        static size_t GetHookSectionSize(span<HookedSymbolEntry> entries);

        void WriteHookSection(span<HookedSymbolEntry> entries, span<u32> hookSection);

        /**
         * @brief An opaque handle to a group of trapped region
         */
        class TrapHandle : private TrapMap::GroupHandle {
            constexpr TrapHandle(const TrapMap::GroupHandle &handle);

            friend NCE;
        };

        /**
         * @brief Creates a region of guest memory that can be trapped with a callback for when an access to it has been made
         * @param lockCallback A callback to lock the resource that is being trapped, it must block until the resource is locked but unlock it prior to returning
         * @param readCallback A callback for read accesses to the trapped region, it must not block and return a boolean if it would block
         * @param writeCallback A callback for write accesses to the trapped region, it must not block and return a boolean if it would block
         * @note The handle **must** be deleted using DeleteTrap before the NCE instance is destroyed
         * @note It is UB to supply a region of host memory rather than guest memory
         * @note This doesn't trap the region in itself, any trapping must be done via TrapRegions(...)
         */
        TrapHandle CreateTrap(span<span<u8>> regions, const LockCallback& lockCallback, const TrapCallback& readCallback, const TrapCallback& writeCallback);

        /**
         * @brief Re-traps a region of memory after protections were removed
         * @param writeOnly If the trap is optimally for write-only accesses, this is not guarenteed
         */
        void TrapRegions(TrapHandle handle, bool writeOnly);

        /**
         * @brief Removes protections from a region of memory
         */
        void RemoveTrap(TrapHandle handle);

        /**
         * @brief Deletes a trap handle and removes the protection from the region
         */
        void DeleteTrap(TrapHandle handle);
    };
}
