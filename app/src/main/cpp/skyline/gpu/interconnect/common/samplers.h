// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <tsl/robin_map.h>
#include "common.h"
#include "tsc.h"

namespace skyline::gpu::interconnect {
    class SamplerPoolState : dirty::CachedManualDirty, dirty::RefreshableManualDirty {
      public:
        struct EngineRegisters {
            const engine_common::TexSamplerPool &texSamplerPool;
            const engine_common::TexHeaderPool &texHeaderPool;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

      public:
        span<TextureSamplerControl> texSamplers;
        bool didUseTexHeaderBinding;

        SamplerPoolState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(InterconnectContext &ctx, bool useTexHeaderBinding);

        bool Refresh(InterconnectContext &ctx, bool useTexHeaderBinding);

        void PurgeCaches();
    };

    class Samplers {
      private:
        dirty::ManualDirtyState<SamplerPoolState> samplerPool;

        tsl::robin_map<TextureSamplerControl, std::unique_ptr<vk::raii::Sampler>, util::ObjectHash<TextureSamplerControl>> texSamplerStore;
        std::vector<vk::raii::Sampler *> texSamplerCache;

      public:
        Samplers(DirtyManager &manager, const SamplerPoolState::EngineRegisters &engine);

        void Update(InterconnectContext &ctx, bool useTexHeaderBinding);

        void MarkAllDirty();

        vk::raii::Sampler *GetSampler(InterconnectContext &ctx, u32 samplerIndex, u32 textureIndex);
    };
}
