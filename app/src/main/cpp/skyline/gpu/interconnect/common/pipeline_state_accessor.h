// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <shader_compiler/shader_info.h>
#include "common.h"
#include "textures.h"

namespace skyline::gpu::interconnect {
    /**
     * @brief Provides an abstract interface for accessing pipeline state at creation time
     */
    class PipelineStateAccessor {
      public:
        /**
         * @return The texture type for the TIC entry at the given index
         */
        virtual Shader::TextureType GetTextureType(u32 index) const = 0;

        /**
         * @return The value of the constant buffer at the given index, offset and stage
         */
        virtual u32 GetConstantBufferValue(u32 shaderStage, u32 index, u32 offset) const = 0;

        /**
         * @return The raw binary for the given pipeline stage
         */
        virtual ShaderBinary GetShaderBinary(u32 pipelineStage) const = 0;

        /**
         * @brief Marks that all Get* operations on the pipeline state has finished and the pipeline is build
         */
        virtual void MarkComplete() = 0;

        virtual ~PipelineStateAccessor() = default;
    };
}
