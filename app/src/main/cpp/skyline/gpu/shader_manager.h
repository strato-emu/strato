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
        Shader::HostTranslateInfo hostTranslateInfo;
        Shader::Profile profile;

      public:
        struct ShaderProgram {
            Shader::IR::Program program;
        };

      private:
        struct SingleShaderProgram : ShaderProgram {
            Shader::ObjectPool<Shader::Maxwell::Flow::Block> flowBlockPool;
            Shader::ObjectPool<Shader::IR::Inst> instructionPool;
            Shader::ObjectPool<Shader::IR::Block> blockPool;

            SingleShaderProgram() = default;

            SingleShaderProgram(const SingleShaderProgram &) = delete;

            SingleShaderProgram &operator=(const SingleShaderProgram &) = delete;
        };

        struct DualVertexShaderProgram : ShaderProgram {
            std::shared_ptr<ShaderProgram> vertexA;
            std::shared_ptr<ShaderProgram> vertexB;

            DualVertexShaderProgram(Shader::IR::Program program, std::shared_ptr<ShaderProgram> vertexA, std::shared_ptr<ShaderProgram> vertexB);

            DualVertexShaderProgram(const DualVertexShaderProgram &) = delete;

            DualVertexShaderProgram &operator=(const DualVertexShaderProgram &) = delete;
        };

      public:
        ShaderManager(const DeviceState &state, GPU &gpu);

        std::shared_ptr<ShaderManager::ShaderProgram> ParseGraphicsShader(Shader::Stage stage, span <u8> binary, u32 baseOffset, u32 bindlessTextureConstantBufferIndex);

        /**
         * @brief Combines the VertexA and VertexB shader programs into a single program
         * @note VertexA/VertexB shader programs must be SingleShaderProgram and not DualVertexShaderProgram
         */
        static std::shared_ptr<ShaderManager::ShaderProgram> CombineVertexShaders(const std::shared_ptr<ShaderProgram> &vertexA, const std::shared_ptr<ShaderProgram> &vertexB, span <u8> vertexBBinary);

        vk::raii::ShaderModule CompileShader(Shader::RuntimeInfo &runtimeInfo, const std::shared_ptr<ShaderProgram> &program, Shader::Backend::Bindings &bindings);
    };
}
