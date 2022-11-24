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
        Shader::ObjectPool<Shader::Maxwell::Flow::Block> flowBlockPool;
        Shader::ObjectPool<Shader::IR::Inst> instructionPool;
        Shader::ObjectPool<Shader::IR::Block> blockPool;
        std::mutex poolMutex;

      public:
        using ConstantBufferRead = std::function<u32(u32 index, u32 offset)>; //!< A function which reads a constant buffer at the specified offset and returns the value
        using GetTextureType = std::function<Shader::TextureType(u32 handle)>; //!< A function which determines the type of a texture from its handle by checking the corresponding TIC

        ShaderManager(const DeviceState &state, GPU &gpu);

        /**
         * @return A shader program that corresponds to all the supplied state including the current state of the constant buffers
         */
        Shader::IR::Program ParseGraphicsShader(const std::array<u32, 8> &postVtgShaderAttributeSkipMask, Shader::Stage stage, span<u8> binary, u32 baseOffset, u32 textureConstantBufferIndex, bool viewportTransformEnabled, const ConstantBufferRead &constantBufferRead, const GetTextureType &getTextureType);

        /**
         * @brief Combines the VertexA and VertexB shader programs into a single program
         * @note VertexA/VertexB shader programs must be SingleShaderProgram and not DualVertexShaderProgram
         */
        Shader::IR::Program CombineVertexShaders(Shader::IR::Program &vertexA, Shader::IR::Program &vertexB, span<u8> vertexBBinary);

        Shader::IR::Program ParseComputeShader(span<u8> binary, u32 baseOffset, u32 textureConstantBufferIndex, u32 localMemorySize, u32 sharedMemorySize, std::array<u32, 3> workgroupDimensions, const ConstantBufferRead &constantBufferRead, const GetTextureType &getTextureType);

        vk::ShaderModule CompileShader(const Shader::RuntimeInfo &runtimeInfo, Shader::IR::Program &program, Shader::Backend::Bindings &bindings);

        void ResetPools();
    };
}
