// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <shader_compiler/shader_info.h>
#include "common.h"

namespace skyline::gpu::interconnect {
    /**
     * @brief Stores both key and non-key state for a pipeline that is otherwise only accessible at creation time
     */
    class PipelineStateBundle {
      private:
        std::vector<u8> key; //!< Byte array containing the pipeline key, this is interpreted by the the user and two different keys might refer to the same pipeline
        std::vector<u8> fileBuffer;

        /**
         * @brief Holds the raw binary and associated info for a pipeline stage
         */
        struct PipelineStage {
            std::vector<u8> binary;
            u32 binaryBaseOffset;

            void Reset();
        };

        /**
         * @brief Holds a value of a constant buffer read from memory at pipeline creation time
         * @note This struct *MUST* not be modified without a pipeline cache version bump
         */
        struct ConstantBufferValue {
            u32 shaderStage;
            u32 index;
            u32 offset;
            u32 value;
        };
        static_assert(sizeof(ConstantBufferValue) == 0x10);

        /**
         * @brief Holds a the texture type of a TIC entry read at pipeline creation time
         * @note This struct *MUST* not be modified without a pipeline cache version bump
         */
        struct TextureTypeEntry {
            u32 index;
            Shader::TextureType type;
        };
        static_assert(sizeof(TextureTypeEntry) == 0x8);

        boost::container::small_vector<ConstantBufferValue, 4> constantBufferValues;
        boost::container::small_vector<TextureTypeEntry, 4> textureTypes;

        std::vector<PipelineStage> pipelineStages{};

      public:
        PipelineStateBundle();

        /**
         * @brief Resets the bundle's state using the given key so it can be reused for a new pipeline
         */
        void Reset(span<const u8> newKey);

        template<typename T> requires std::is_trivially_copyable_v<T> && (!requires (T t){ t.size(); })
        void Reset(const T &value) {
            Reset(span<const u8>(reinterpret_cast<const u8 *>(&value), sizeof(T)));
        }

        /**
         * @brief Sets the binary for a given pipeline stage
         */
        void SetShaderBinary(u32 pipelineStage, ShaderBinary bin);

        /**
         * @brief Adds a texture type value for a given offset to the bundle
         */
        void AddTextureType(u32 index, Shader::TextureType type);

        /**
         * @brief Adds a constant buffer value for a given offset and shader stage to the bundle
         */
        void AddConstantBufferValue(u32 shaderStage, u32 index, u32 offset, u32 value);

        /**
         * @brief Returns the raw key data for the pipeline
         */
        span<u8> GetKey();

        template<typename T> requires std::is_trivially_copyable_v<T>
        const T &GetKey() {
            return GetKey().as<T>();
        }

        /**
         * @brief Returns the binary for a given pipeline stage
         */
        ShaderBinary GetShaderBinary(u32 pipelineStage);

        /**
         * @brief Returns the texture type for a given offset
         */
        Shader::TextureType LookupTextureType(u32 index);

        /**
         * @brief Returns the constant buffer value for a given offset and shader stage
         */
        u32 LookupConstantBufferValue(u32 shaderStage, u32 index, u32 offset);

        bool Deserialise(std::ifstream &stream);

        void Serialise(std::ofstream &stream);
    };
}
