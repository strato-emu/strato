// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <tsl/robin_map.h>
#include "common.h"
#include "tsc.h"

namespace skyline::gpu::interconnect::maxwell3d {
    class SamplerPoolState : dirty::CachedManualDirty {
      public:
        struct EngineRegisters {
            const engine::SamplerBinding &samplerBinding;
            const engine::TexSamplerPool &texSamplerPool;
            const engine::TexHeaderPool &texHeaderPool;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

      public:
        span<TextureSamplerControl> texSamplers;
        bool useTexHeaderBinding;

        SamplerPoolState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(InterconnectContext &ctx);

        void PurgeCaches();
    };

    class Samplers {
      private:
        dirty::ManualDirtyState<SamplerPoolState> samplerPool;

        tsl::robin_map<TextureSamplerControl, std::unique_ptr<vk::raii::Sampler>, util::ObjectHash<TextureSamplerControl>> texSamplerStore;
        std::vector<vk::raii::Sampler *> texSamplerCache;

      public:
        Samplers(DirtyManager &manager, const SamplerPoolState::EngineRegisters &engine);

        void MarkAllDirty();

        vk::raii::Sampler *GetSampler(InterconnectContext &ctx, u32 samplerIndex, u32 textureIndex);
    };
}
