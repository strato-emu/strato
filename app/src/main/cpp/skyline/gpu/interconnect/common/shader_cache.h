// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <tsl/robin_map.h>
#include "common.h"

namespace skyline::gpu::interconnect {
    /**
     * @brief Caches guest shader binaries and their memory locations
     */
    class ShaderCache {
      private:
        /**
         * @brief Holds mirror state for a single GPU mapped block
         */
        struct MirrorEntry {
            span<u8> mirror;
            tsl::robin_map<u8 *, std::pair<ShaderBinary, u64>> cache;
            std::optional<nce::NCE::TrapHandle> trap;

            static constexpr u32 SkipTrapThreshold{20}; //!< Threshold for the number of times a mirror trap needs to be hit before we fallback to always hashing
            u32 trapCount{}; //!< The number of times the trap has been hit, used to avoid trapping in cases where the constant retraps would harm performance
            ContextTag executionTag{}; //!< For the case where `trapCount > SkipTrapThreshold`, the memory sequence number number used to clear the cache after every access
            bool dirty{}; //!< If the trap has been hit and the cache needs to be cleared

            MirrorEntry(span<u8> alignedMirror) : mirror{alignedMirror} {}
        };

        tsl::robin_map<u8 *, std::unique_ptr<MirrorEntry>> mirrorMap;
        std::recursive_mutex trapMutex; //!< Protects accesses from trap handlers to the mirror map
        std::optional<std::scoped_lock<std::recursive_mutex>> trapExecutionLock; //!< Persistently held lock over an execution to avoid frequent relocking
        MirrorEntry *entry{};
        span<u8> mirrorBlock{}; //!< Guest mapped memory block corresponding to `entry`
        u64 lastProgramBase{};
        u32 lastProgramOffset{};
        std::vector<u8> splitBinaryStorage;

      public:
        /**
         * @brief Returns the shader binary located at the given address
         */
        std::pair<ShaderBinary, u64> Lookup(InterconnectContext &ctx, u64 programBase, u32 programOffset);

        bool Refresh(InterconnectContext &ctx, u64 programBase, u32 programOffset);

        void PurgeCaches();
    };
}
