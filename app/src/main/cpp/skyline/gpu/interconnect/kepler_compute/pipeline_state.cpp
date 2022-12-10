// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include "pipeline_state.h"

namespace skyline::gpu::interconnect::kepler_compute {
    /* Pipeline Stage */
    void PipelineStageState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, programRegion);
    }

    PipelineStageState::PipelineStageState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine)
        : engine{manager, dirtyHandle, engine} {}

    void PipelineStageState::Flush(InterconnectContext &ctx, u32 programOffset) {
        std::tie(binary, hash) = cache.Lookup(ctx, engine->programRegion, programOffset);
    }

    bool PipelineStageState::Refresh(InterconnectContext &ctx, u32 programOffset) {
        return cache.Refresh(ctx, engine->programRegion, programOffset);
    }

    void PipelineStageState::PurgeCaches() {
        cache.PurgeCaches();
    }

    /* Pipeline State */
    PipelineState::PipelineState(DirtyManager &manager, const EngineRegisters &engine)
        : pipelineStage{manager, engine.pipelineStageRegisters},
          bindlessTexture{engine.bindlessTexture} {}

    Pipeline *PipelineState::Update(InterconnectContext &ctx, StateUpdateBuilder &builder, Textures &textures, ConstantBufferSet &constantBuffers, const QMD &qmd) {
        const auto &stage{pipelineStage.UpdateGet(ctx, qmd.programOffset)};
        packedState.shaderHash = stage.hash;
        packedState.dimensions = {qmd.ctaThreadDimension0, qmd.ctaThreadDimension1, qmd.ctaThreadDimension2};
        packedState.localMemorySize = qmd.shaderLocalMemoryLowSize + qmd.shaderLocalMemoryHighSize;
        packedState.sharedMemorySize = qmd.sharedMemorySize;
        packedState.bindlessTextureConstantBufferSlotSelect = bindlessTexture.constantBufferSlotSelect;

        return ctx.gpu.computePipelineManager.FindOrCreate(ctx, textures, constantBuffers, packedState, stage.binary);
    }

    void PipelineState::PurgeCaches() {
        pipelineStage.MarkDirty(true);
    }
}
