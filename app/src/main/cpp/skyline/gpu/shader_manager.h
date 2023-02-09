// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <unordered_map>
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
        std::unordered_map<u64, std::vector<u8>> guestShaderReplacements; //!< Map of guest shader hash -> replacement guest shader binary, populated at init time and must not be modified after
        std::unordered_map<u64, std::vector<u8>> hostShaderReplacements; //!< ^^ same as above but for host

        std::mutex poolMutex;
        std::filesystem::path dumpPath;
        std::mutex dumpMutex;

        /**
         * @brief Called at init time to populate the shader replacements map from the input directory
         */
        void LoadShaderReplacements(std::string_view replacementDir);

        /**
         * @brief Returns the raw binary of shader replacement for the given hash, if no replacement is found the input binary is returned
         * @note This will also dump the binary to disk if dumping is enabled
         */
        span<u8> ProcessShaderBinary(bool spv, u64 hash, span<u8> binary);

      public:
        using ConstantBufferRead = std::function<u32(u32 index, u32 offset)>; //!< A function which reads a constant buffer at the specified offset and returns the value
        using GetTextureType = std::function<Shader::TextureType(u32 handle)>; //!< A function which determines the type of a texture from its handle by checking the corresponding TIC

        ShaderManager(const DeviceState &state, GPU &gpu, std::string_view replacementDir, std::string_view dumpDir);

        /**
         * @return A shader program that corresponds to all the supplied state including the current state of the constant buffers
         */
        Shader::IR::Program ParseGraphicsShader(const std::array<u32, 8> &postVtgShaderAttributeSkipMask, Shader::Stage stage, u64 hash, span<u8> binary, u32 baseOffset, u32 textureConstantBufferIndex, bool viewportTransformEnabled, const ConstantBufferRead &constantBufferRead, const GetTextureType &getTextureType);

        /**
         * @brief Combines the VertexA and VertexB shader programs into a single program
         * @note VertexA/VertexB shader programs must be SingleShaderProgram and not DualVertexShaderProgram
         */
        Shader::IR::Program CombineVertexShaders(Shader::IR::Program &vertexA, Shader::IR::Program &vertexB, span<u8> vertexBBinary);

        /**
         * @brief Generates a passthrough geometry shader to write gl_Layer on devices that don't support VK_EXT_shader_viewport_index_layer
         */
        Shader::IR::Program GenerateGeometryPassthroughShader(Shader::IR::Program &layerSource, Shader::OutputTopology topology);

        Shader::IR::Program ParseComputeShader(u64 hash, span<u8> binary, u32 baseOffset, u32 textureConstantBufferIndex, u32 localMemorySize, u32 sharedMemorySize, std::array<u32, 3> workgroupDimensions, const ConstantBufferRead &constantBufferRead, const GetTextureType &getTextureType);

        vk::ShaderModule CompileShader(const Shader::RuntimeInfo &runtimeInfo, Shader::IR::Program &program, Shader::Backend::Bindings &bindings, u64 hash = 0);

        void ResetPools();
    };
}
