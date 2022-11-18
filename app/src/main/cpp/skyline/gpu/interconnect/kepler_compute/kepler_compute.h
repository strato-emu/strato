// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <gpu/descriptor_allocator.h>
#include <gpu/interconnect/common/samplers.h>
#include <gpu/interconnect/common/textures.h>
#include "constant_buffers.h"
#include "pipeline_state.h"

namespace skyline::gpu::interconnect::kepler_compute {
    /**
     * @brief The core Kepler Compute interconnect object, directly accessed by the engine code to perform compute dispatches
     */
    class KeplerCompute {
      public:
        /**
         * @brief The full set of register state used by the GPU interconnect
         */
        struct EngineRegisterBundle {
            PipelineState::EngineRegisters pipelineStateRegisters;
            SamplerPoolState::EngineRegisters samplerPoolRegisters;
            TexturePoolState::EngineRegisters texturePoolRegisters;
        };

      private:
        InterconnectContext ctx;
        PipelineState pipelineState;
        ConstantBuffers constantBuffers;
        Samplers samplers;
        Textures textures;

      public:
        KeplerCompute(GPU &gpu,
                      soc::gm20b::ChannelContext &channelCtx,
                      nce::NCE &nce,
                      kernel::MemoryManager &memoryManager,
                      DirtyManager &manager,
                      const EngineRegisterBundle &registerBundle);

        /**
         * @brief Performs a compute dispatch using the given QMD
         */
        void Dispatch(const QMD &qmd);
    };
}
