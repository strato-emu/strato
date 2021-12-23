// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vulkan/vulkan.hpp>
#include <shader_compiler/object_pool.h>
#include <shader_compiler/frontend/maxwell/control_flow.h>
#include <shader_compiler/frontend/ir/value.h>
#include <shader_compiler/frontend/ir/basic_block.h>
#include <shader_compiler/frontend/ir/program.h>
#include <shader_compiler/host_translate_info.h>
#include <shader_compiler/profile.h>
#include <shader_compiler/runtime_info.h>
#include <shader_compiler/backend/bindings.h>
#include <common.h>

namespace skyline::gpu {
    /**
     * @brief The Shader Manager is responsible for caching and looking up shaders alongside handling compilation of shaders when not found in any cache
     */
    class ShaderManager {
      private:
        GPU &gpu;
        Shader::ObjectPool<Shader::Maxwell::Flow::Block> flowBlockPool;
        Shader::ObjectPool<Shader::IR::Inst> instPool;
        Shader::ObjectPool<Shader::IR::Block> blockPool;
        Shader::HostTranslateInfo hostTranslateInfo;
        Shader::Profile profile;

      public:
        ShaderManager(const DeviceState &state, GPU &gpu);

        Shader::IR::Program ParseGraphicsShader(span<u8> binary, Shader::Stage stage, u32 baseOffset);

        /**
         * @brief Combines the VertexA and VertexB shader programs into a single program
         */
        static Shader::IR::Program CombineVertexShaders(Shader::IR::Program &vertexA, Shader::IR::Program &vertexB, span<u8> vertexBBinary);

        vk::raii::ShaderModule CompileShader(Shader::RuntimeInfo &runtimeInfo, Shader::IR::Program &program, Shader::Backend::Bindings &bindings);
    };
}
