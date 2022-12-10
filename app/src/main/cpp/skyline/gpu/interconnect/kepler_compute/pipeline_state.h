// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <gpu/texture/texture.h>
#include <gpu/interconnect/common/shader_cache.h>
#include "common.h"
#include "packed_pipeline_state.h"
#include "pipeline_manager.h"

namespace skyline::gpu::interconnect::kepler_compute {
    class PipelineStageState : dirty::RefreshableManualDirty, dirty::CachedManualDirty {
      public:
        struct EngineRegisters {
            const soc::gm20b::engine::Address &programRegion;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

        ShaderCache cache;

      public:
        ShaderBinary binary;
        u64 hash{};

        PipelineStageState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);
        
        void Flush(InterconnectContext &ctx, u32 programOffset);

        bool Refresh(InterconnectContext &ctx, u32 programOffset);

        void PurgeCaches();
    };

    class PipelineState {
      public:
        struct EngineRegisters {
            PipelineStageState::EngineRegisters pipelineStageRegisters;
            const engine_common::BindlessTexture &bindlessTexture;
        };

      private:
        dirty::ManualDirtyState<PipelineStageState> pipelineStage;
        const engine_common::BindlessTexture &bindlessTexture;

        PackedPipelineState packedState{};

      public:
        PipelineState(DirtyManager &manager, const EngineRegisters &engine);

        Pipeline *Update(InterconnectContext &ctx, StateUpdateBuilder &builder, Textures &textures, ConstantBufferSet &constantBuffers, const QMD &qmd);

        void PurgeCaches();
    };
}
