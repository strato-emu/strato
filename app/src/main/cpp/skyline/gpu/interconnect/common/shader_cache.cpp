// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/Ryujinx/)
// Copyright © 2022 yuzu Team and Contributors (https://github.com/yuzu-emu/)
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <nce.h>
#include <kernel/memory.h>
#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include <gpu.h>
#include "shader_cache.h"

namespace skyline::gpu::interconnect {
    /* Pipeline Stage */
    std::pair<ShaderBinary, u64> ShaderCache::Lookup(InterconnectContext &ctx, u64 programBase, u32 programOffset) {
        lastProgramBase = programBase;
        lastProgramOffset = programOffset;
        auto[blockMapping, blockOffset]{ctx.channelCtx.asCtx->gmmu.LookupBlock(programBase + programOffset)};

        if (!trapExecutionLock)
            trapExecutionLock.emplace(trapMutex);

        // Skip looking up the mirror if it is the same as the one used for the previous update
        if (!mirrorBlock.valid() || !mirrorBlock.contains(blockMapping)) {
            auto mirrorIt{mirrorMap.find(blockMapping.data())};
            if (mirrorIt == mirrorMap.end()) {
                // Allocate a host mirror for the mapping and trap the guest region
                auto newIt{mirrorMap.emplace(blockMapping.data(), std::make_unique<MirrorEntry>(ctx.memory.CreateMirror(blockMapping)))};

                // We need to create the trap after allocating the entry so that we have an `invalid` pointer we can pass in
                auto trapHandle{ctx.nce.CreateTrap(blockMapping, [mutex = &trapMutex]() {
                    std::scoped_lock lock{*mutex};
                    return;
                }, []() { return true; }, [entry = newIt.first->second.get(), mutex = &trapMutex]() {
                    std::unique_lock lock{*mutex, std::try_to_lock};
                    if (!lock)
                        return false;

                    if (++entry->trapCount <= MirrorEntry::SkipTrapThreshold)
                        entry->dirty = true;
                    return true;
                })};

                // Write only trap
                ctx.nce.TrapRegions(trapHandle, true);

                entry = newIt.first->second.get();
                entry->trap = trapHandle;
            } else {
                entry = mirrorIt->second.get();
            }

            mirrorBlock = blockMapping;
        }

        if (entry->trapCount > MirrorEntry::SkipTrapThreshold && entry->executionTag != ctx.executor.executionTag) {
            entry->executionTag = ctx.executor.executionTag;
            entry->dirty = true;
        }

        // If the mirror entry has been written to, clear its shader binary cache and retrap to catch any future writes
        if (entry->dirty || ctx.executor.usageTracker.sequencedIntervals.Intersect(blockMapping.subspan(blockOffset))) {
            entry->cache.clear();
            entry->dirty = false;

            if (entry->trapCount <= MirrorEntry::SkipTrapThreshold)
                ctx.nce.TrapRegions(*entry->trap, true);
        } else if (auto it{entry->cache.find(blockMapping.data() + blockOffset)}; it != entry->cache.end()) {
            return it->second;
        }

        // entry->mirror may not be a direct mirror of blockMapping and may just contain it as a subregion, so we need to explicitly calculate the offset
        span<u8> blockMappingMirror{blockMapping.data() - mirrorBlock.data() + entry->mirror.data(), blockMapping.size()};

        ShaderBinary binary{};
        auto shaderSubmapping{blockMappingMirror.subspan(blockOffset)};
        // If nothing was in the cache then do a full shader parse
        binary.binary = [](span<u8> mapping) {
            // We attempt to find the shader size by looking for "BRA $" (Infinite Loop) which is used as padding at the end of the shader
            // UAM Shader Compiler Reference: https://github.com/devkitPro/uam/blob/5a5afc2bae8b55409ab36ba45be63fcb73f68993/source/compiler_iface.cpp#L319-L351
            constexpr u64 BraSelf1{0xE2400FFFFF87000F}, BraSelf2{0xE2400FFFFF07000F};

            span<u64> shaderInstructions{mapping.cast<u64, std::dynamic_extent, true>()};
            for (auto it{shaderInstructions.begin()}; it != shaderInstructions.end(); it++) {
                auto instruction{*it};
                if (instruction == BraSelf1 || instruction == BraSelf2) [[unlikely]]
                    // It is far more likely that the instruction doesn't match so this is an unlikely case
                    return span{shaderInstructions.begin(), it}.cast<u8>();
            }

            return span<u8>{};
        }(shaderSubmapping);

        if (!binary.binary.valid()) {
            static constexpr size_t FallbackSize{0x10000}; //!< Fallback shader size for when we can't detect it with the BRA $ pattern
            if (shaderSubmapping.size() > FallbackSize) {
                binary.binary = shaderSubmapping;
            } else {
                // If the shader is split across multiple mappings then read into an internal vector to store the binary
                size_t storageOffset{splitBinaryStorage.size()};
                splitBinaryStorage.resize(storageOffset + FallbackSize);
                auto shaderStorage{span{splitBinaryStorage}.subspan(storageOffset, FallbackSize)};
                auto mappings{ctx.channelCtx.asCtx->gmmu.TranslateRange(programBase + programOffset, storageOffset + FallbackSize)};
                u8 *shaderStoragePtr{shaderStorage.data()};
                for (auto mapping : mappings) {
                    if (!mapping.valid())
                        break;

                    std::memcpy(shaderStoragePtr, mapping.data(), mapping.size());
                    shaderStoragePtr += mapping.size();
                }

                binary.binary = shaderStorage;
            }
        }

        binary.baseOffset = programOffset;

        u64 hash{XXH64(binary.binary.data(), binary.binary.size_bytes(), 0)};
        entry->cache.insert({blockMapping.data() + blockOffset, {binary, hash}});

        return {binary, hash};
    }

    bool ShaderCache::Refresh(InterconnectContext &ctx, u64 programBase, u32 programOffset) {
        if (!trapExecutionLock)
            trapExecutionLock.emplace(trapMutex);

        if (programBase != lastProgramBase || programOffset != lastProgramOffset)
            return true;

        if (entry && entry->trapCount > MirrorEntry::SkipTrapThreshold && entry->executionTag != ctx.executor.executionTag)
            return true;
        else if (entry && entry->dirty)
            return true;

        return false;
    }

    void ShaderCache::PurgeCaches() {
        trapExecutionLock.reset();
    }
}
