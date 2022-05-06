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
        std::mutex programMutex; //!< Synchronizes accesses to the program caches
        std::mutex moduleMutex; //!< Synchronizes accesses to the module cache

      public:
        using ConstantBufferRead = std::function<u32(u32 index, u32 offset)>; //!< A function which reads a constant buffer at the specified offset and returns the value

        /**
         * @brief A single u32 word from a constant buffer with the offset it was read from, utilized to ensure constant buffer state is consistent
         */
        struct ConstantBufferWord {
            u32 index; //!< The index of the constant buffer
            u32 offset; //!< The offset of the constant buffer word
            u32 value; //!< The contents of the word

            constexpr ConstantBufferWord(u32 index, u32 offset, u32 value);

            constexpr bool operator==(const ConstantBufferWord &other) const = default;
        };

        using GetTextureType = std::function<Shader::TextureType(u32 handle)>; //!< A function which determines the type of a texture from its handle by checking the corresponding TIC

        struct CachedTextureType {
            u32 handle;
            Shader::TextureType type;

            constexpr CachedTextureType(u32 handle, Shader::TextureType type);

            constexpr bool operator==(const CachedTextureType &other) const = default;
        };

        struct ShaderProgram {
            Shader::IR::Program program;

            ShaderProgram() = default;

            ShaderProgram(Shader::IR::Program &&program);

            /**
             * @return If the current state match the expected values, if they don't match then the shader program might be inaccurate for the current behavior
             */
            virtual bool VerifyState(const ConstantBufferRead &constantBufferRead, const GetTextureType &getTextureType) const = 0;
        };

      private:
        struct SingleShaderProgram final : ShaderProgram {
            Shader::ObjectPool<Shader::Maxwell::Flow::Block> flowBlockPool;
            Shader::ObjectPool<Shader::IR::Inst> instructionPool;
            Shader::ObjectPool<Shader::IR::Block> blockPool;

            std::vector<ConstantBufferWord> constantBufferWords;
            std::vector<CachedTextureType> textureTypes;

            SingleShaderProgram() = default;

            SingleShaderProgram(const SingleShaderProgram &) = delete;

            SingleShaderProgram &operator=(const SingleShaderProgram &) = delete;

            bool VerifyState(const ConstantBufferRead &constantBufferRead, const GetTextureType &getTextureType) const final;
        };

        struct GuestShaderKey {
            Shader::Stage stage;
            std::vector<u8> bytecode;
            u32 textureConstantBufferIndex;
            span<ConstantBufferWord> constantBufferWords;
            span<CachedTextureType> textureTypes;

            GuestShaderKey(Shader::Stage stage, span<u8> bytecode, u32 textureConstantBufferIndex, span<ConstantBufferWord> constantBufferWords, span<CachedTextureType> textureTypes);
        };

        struct GuestShaderLookup {
            Shader::Stage stage;
            span<u8> bytecode;
            u32 textureConstantBufferIndex;
            ConstantBufferRead constantBufferRead;
            GetTextureType getTextureType;

            GuestShaderLookup(Shader::Stage stage, span<u8> bytecode, u32 textureConstantBufferIndex, ConstantBufferRead constantBufferRead, GetTextureType getTextureType);
        };

        struct ShaderProgramHash {
            using is_transparent = std::true_type;

            size_t operator()(const GuestShaderKey &key) const noexcept;

            size_t operator()(const GuestShaderLookup &key) const noexcept;
        };

        struct ShaderProgramEqual {
            using is_transparent = std::true_type;

            bool operator()(const GuestShaderKey &lhs, const GuestShaderLookup &rhs) const noexcept;

            bool operator()(const GuestShaderKey &lhs, const GuestShaderKey &rhs) const noexcept;
        };

        std::unordered_map<GuestShaderKey, std::shared_ptr<SingleShaderProgram>, ShaderProgramHash, ShaderProgramEqual> programCache; //!< A map from Maxwell bytecode to the corresponding shader program

        struct DualVertexShaderProgram final : ShaderProgram {
            std::shared_ptr<ShaderProgram> vertexA;
            std::shared_ptr<ShaderProgram> vertexB;

            DualVertexShaderProgram(Shader::IR::Program program, std::shared_ptr<ShaderProgram> vertexA, std::shared_ptr<ShaderProgram> vertexB);

            DualVertexShaderProgram(const DualVertexShaderProgram &) = delete;

            DualVertexShaderProgram &operator=(const DualVertexShaderProgram &) = delete;

            bool VerifyState(const ConstantBufferRead &constantBufferRead, const GetTextureType &getTextureType) const final;
        };

        using DualVertexPrograms = std::pair<std::shared_ptr<ShaderProgram>, std::shared_ptr<ShaderProgram>>;

        struct DualVertexProgramsHash {
            constexpr size_t operator()(const std::pair<std::shared_ptr<ShaderProgram>, std::shared_ptr<ShaderProgram>> &p) const;
        };

        std::unordered_map<DualVertexPrograms, std::shared_ptr<DualVertexShaderProgram>, DualVertexProgramsHash> dualProgramCache; //!< A map from Vertex A and Vertex B shader programs to the corresponding dual vertex shader program

        /**
         * @brief All unique state that is required to compile a shader program, this is used as the key for the associative compiled shader program cache
         */
        struct ShaderModuleState {
            std::shared_ptr<ShaderProgram> program;
            Shader::Backend::Bindings bindings; //!< The bindings prior to the shader being compiled
            Shader::RuntimeInfo runtimeInfo;

            bool operator==(const ShaderModuleState &) const;
        };

        struct ShaderModuleStateHash {
            constexpr size_t operator()(const ShaderModuleState &state) const;
        };

        struct ShaderModule {
            vk::raii::ShaderModule vkModule;
            Shader::Backend::Bindings bindings; //!< The bindings after the shader has been compiled

            ShaderModule(const vk::raii::Device &device, const vk::ShaderModuleCreateInfo &createInfo, Shader::Backend::Bindings bindings);
        };

        std::unordered_map<ShaderModuleState, ShaderModule, ShaderModuleStateHash> shaderModuleCache; //!< A map from shader module state to the corresponding Vulkan shader module

      public:
        ShaderManager(const DeviceState &state, GPU &gpu);

        /**
         * @return A shader program that corresponds to all the supplied state including the current state of the constant buffers
         */
        std::shared_ptr<ShaderManager::ShaderProgram> ParseGraphicsShader(Shader::Stage stage, span<u8> binary, u32 baseOffset, u32 textureConstantBufferIndex, const ConstantBufferRead &constantBufferRead, const GetTextureType &getTextureType);

        /**
         * @brief Combines the VertexA and VertexB shader programs into a single program
         * @note VertexA/VertexB shader programs must be SingleShaderProgram and not DualVertexShaderProgram
         */
        std::shared_ptr<ShaderManager::ShaderProgram> CombineVertexShaders(const std::shared_ptr<ShaderProgram> &vertexA, const std::shared_ptr<ShaderProgram> &vertexB, span<u8> vertexBBinary);

        vk::ShaderModule CompileShader(Shader::RuntimeInfo &runtimeInfo, const std::shared_ptr<ShaderProgram> &program, Shader::Backend::Bindings &bindings);
    };
}
