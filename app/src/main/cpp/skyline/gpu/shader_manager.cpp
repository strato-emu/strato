// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <range/v3/algorithm.hpp>
#include <boost/functional/hash.hpp>
#include <gpu.h>
#include <shader_compiler/common/settings.h>
#include <shader_compiler/common/log.h>
#include <shader_compiler/frontend/maxwell/translate_program.h>
#include <shader_compiler/backend/spirv/emit_spirv.h>
#include <vulkan/vulkan_raii.hpp>
#include "shader_manager.h"

namespace Shader::Log {
    void Debug(const std::string &message) {
        skyline::Logger::Write(skyline::Logger::LogLevel::Debug, message);
    }

    void Warn(const std::string &message) {
        skyline::Logger::Write(skyline::Logger::LogLevel::Warn, message);
    }

    void Error(const std::string &message) {
        skyline::Logger::Write(skyline::Logger::LogLevel::Error, message);
    }
}

namespace skyline::gpu {
    ShaderManager::ShaderManager(const DeviceState &state, GPU &gpu) : gpu{gpu} {
        auto &traits{gpu.traits};
        hostTranslateInfo = Shader::HostTranslateInfo{
            .support_float16 = traits.supportsFloat16,
            .support_int64 = traits.supportsInt64,
            .needs_demote_reorder = false,
            .support_snorm_render_buffer = true
        };

        constexpr u32 TegraX1WarpSize{32}; //!< The amount of threads in a warp on the Tegra X1
        profile = Shader::Profile{
            .supported_spirv = traits.supportsSpirv14 ? 0x00010400U : 0x00010000U,
            .unified_descriptor_binding = true,
            .support_descriptor_aliasing = !traits.quirks.brokenDescriptorAliasing,
            .support_int8 = traits.supportsInt8,
            .support_int16 = traits.supportsInt16,
            .support_int64 = traits.supportsInt64,
            .support_vertex_instance_id = false,
            .support_float_controls = traits.supportsFloatControls,
            .support_separate_denorm_behavior = traits.floatControls.denormBehaviorIndependence == vk::ShaderFloatControlsIndependence::eAll,
            .support_separate_rounding_mode = traits.floatControls.roundingModeIndependence == vk::ShaderFloatControlsIndependence::eAll,
            .support_fp16_denorm_preserve = static_cast<bool>(traits.floatControls.shaderDenormPreserveFloat16),
            .support_fp32_denorm_preserve = static_cast<bool>(traits.floatControls.shaderDenormPreserveFloat32),
            .support_fp16_denorm_flush = static_cast<bool>(traits.floatControls.shaderDenormFlushToZeroFloat16),
            .support_fp32_denorm_flush = static_cast<bool>(traits.floatControls.shaderDenormFlushToZeroFloat32),
            .support_fp16_signed_zero_nan_preserve = static_cast<bool>(traits.floatControls.shaderSignedZeroInfNanPreserveFloat16),
            .support_fp32_signed_zero_nan_preserve = static_cast<bool>(traits.floatControls.shaderSignedZeroInfNanPreserveFloat32),
            .support_fp64_signed_zero_nan_preserve = static_cast<bool>(traits.floatControls.shaderSignedZeroInfNanPreserveFloat64),
            .support_explicit_workgroup_layout = false,
            .support_vote = traits.supportsSubgroupVote,
            .support_viewport_index_layer_non_geometry = traits.supportsShaderViewportIndexLayer,
            .support_viewport_mask = false,
            .support_typeless_image_loads = traits.supportsImageReadWithoutFormat,
            .support_demote_to_helper_invocation = traits.supportsShaderDemoteToHelper,
            .support_int64_atomics = traits.supportsAtomicInt64,
            .support_derivative_control = true,
            .support_geometry_shader_passthrough = false,
            .warp_size_potentially_larger_than_guest = TegraX1WarpSize < traits.subgroupSize,
            .lower_left_origin_mode = false,
            .need_declared_frag_colors = false,
            .has_broken_spirv_position_input = traits.quirks.brokenSpirvPositionInput
        };

        Shader::Settings::values = {
            #ifdef NDEBUG
            .renderer_debug = false,
            .disable_shader_loop_safety_checks = false,
            #else
            .renderer_debug = true,
            .disable_shader_loop_safety_checks = true,
            #endif
            .resolution_info = {
                .active = false,
            },
        };
    }

    /**
     * @brief A shader environment for all graphics pipeline stages
     */
    class GraphicsEnvironment : public Shader::Environment {
      private:
        span<u8> binary;
        u32 baseOffset;
        u32 textureBufferIndex;
        bool viewportTransformEnabled;
        ShaderManager::ConstantBufferRead constantBufferRead;
        ShaderManager::GetTextureType getTextureType;

      public:
        GraphicsEnvironment(const std::array<u32, 8> &postVtgShaderAttributeSkipMask,
                            Shader::Stage pStage,
                            span<u8> pBinary, u32 baseOffset,
                            u32 textureBufferIndex,
                            bool viewportTransformEnabled,
                            ShaderManager::ConstantBufferRead constantBufferRead, ShaderManager::GetTextureType getTextureType)
            : binary{pBinary}, baseOffset{baseOffset},
              textureBufferIndex{textureBufferIndex},
              viewportTransformEnabled{viewportTransformEnabled},
              constantBufferRead{std::move(constantBufferRead)}, getTextureType{std::move(getTextureType)} {
            gp_passthrough_mask = postVtgShaderAttributeSkipMask;
            stage = pStage;
            sph = *reinterpret_cast<Shader::ProgramHeader *>(binary.data());
            start_address = baseOffset;
        }

        [[nodiscard]] u64 ReadInstruction(u32 address) final {
            address -= baseOffset;
            if (binary.size() < (address + sizeof(u64)))
                throw exception("Out of bounds instruction read: 0x{:X}", address);
            return *reinterpret_cast<u64 *>(binary.data() + address);
        }

        [[nodiscard]] u32 ReadCbufValue(u32 index, u32 offset) final {
            return constantBufferRead(index, offset);
        }

        [[nodiscard]] Shader::TexturePixelFormat ReadTexturePixelFormat(u32 handle) final {
            throw exception("ReadTexturePixelFormat not implemented");
        }

        [[nodiscard]] Shader::TextureType ReadTextureType(u32 handle) final {
            return getTextureType(handle);
        }

        [[nodiscard]] u32 ReadViewportTransformState() final {
            return viewportTransformEnabled ? 1 : 0; // Only relevant for graphics shaders
        }

        [[nodiscard]] u32 TextureBoundBuffer() const final {
            return textureBufferIndex;
        }

        [[nodiscard]] u32 LocalMemorySize() const final {
            return static_cast<u32>(sph.LocalMemorySize()) + sph.common3.shader_local_memory_crs_size;
        }

        [[nodiscard]] u32 SharedMemorySize() const final {
            return 0; // Only relevant for compute shaders
        }

        [[nodiscard]] std::array<u32, 3> WorkgroupSize() const final {
            return {0, 0, 0}; // Only relevant for compute shaders
        }

        void Dump(u64 hash) final {}
    };

    /**
     * @brief A shader environment for all compute pipeline stages
     */
    class ComputeEnvironment : public Shader::Environment {
      private:
        span<u8> binary;
        u32 baseOffset;
        u32 textureBufferIndex;
        u32 localMemorySize;
        u32 sharedMemorySize;
        std::array<u32, 3> workgroupDimensions;
        ShaderManager::ConstantBufferRead constantBufferRead;
        ShaderManager::GetTextureType getTextureType;

      public:
        ComputeEnvironment(span<u8> pBinary,
                           u32 baseOffset,
                           u32 textureBufferIndex,
                           u32 localMemorySize, u32 sharedMemorySize,
                           std::array<u32, 3> workgroupDimensions,
                           ShaderManager::ConstantBufferRead constantBufferRead, ShaderManager::GetTextureType getTextureType)
            : binary{pBinary},
              baseOffset{baseOffset},
              textureBufferIndex{textureBufferIndex},
              localMemorySize{localMemorySize},
              sharedMemorySize{sharedMemorySize},
              workgroupDimensions{workgroupDimensions},
              constantBufferRead{std::move(constantBufferRead)},
              getTextureType{std::move(getTextureType)} {
            stage = Shader::Stage::Compute;
            start_address = baseOffset;
        }

        [[nodiscard]] u64 ReadInstruction(u32 address) final {
            address -= baseOffset;
            if (binary.size() < (address + sizeof(u64)))
                throw exception("Out of bounds instruction read: 0x{:X}", address);
            return *reinterpret_cast<u64 *>(binary.data() + address);
        }

        [[nodiscard]] u32 ReadCbufValue(u32 index, u32 offset) final {
            return constantBufferRead(index, offset);
        }

        [[nodiscard]] Shader::TexturePixelFormat ReadTexturePixelFormat(u32 handle) final {
            throw exception("ReadTexturePixelFormat not implemented");
        }

        [[nodiscard]] Shader::TextureType ReadTextureType(u32 handle) final {
            return getTextureType(handle);
        }

        [[nodiscard]] u32 ReadViewportTransformState() final {
            return 0; // Only relevant for graphics shaders
        }

        [[nodiscard]] u32 TextureBoundBuffer() const final {
            return textureBufferIndex;
        }

        [[nodiscard]] u32 LocalMemorySize() const final {
            return localMemorySize;
        }

        [[nodiscard]] u32 SharedMemorySize() const final {
            return sharedMemorySize;
        }

        [[nodiscard]] std::array<u32, 3> WorkgroupSize() const final {
            return workgroupDimensions;
        }

        void Dump(u64 hash) final {}
    };


    /**
     * @brief A shader environment for VertexB during combination as it only requires the shader header and no higher level context
     */
    class VertexBEnvironment : public Shader::Environment {
      public:
        explicit VertexBEnvironment(span<u8> binary) {
            sph = *reinterpret_cast<Shader::ProgramHeader *>(binary.data());
            stage = Shader::Stage::VertexB;
        }

        [[nodiscard]] u64 ReadInstruction(u32 address) final {
            throw exception("Not implemented");
        }

        [[nodiscard]] u32 ReadCbufValue(u32 index, u32 offset) final {
            throw exception("Not implemented");
        }

        [[nodiscard]] Shader::TextureType ReadTextureType(u32 handle) final {
            throw exception("Not implemented");
        }

        [[nodiscard]] Shader::TexturePixelFormat ReadTexturePixelFormat(u32 handle) final {
            throw exception("Not implemented");
        }

        [[nodiscard]] u32 ReadViewportTransformState() final {
            throw exception("Not implemented");
        }

        [[nodiscard]] u32 TextureBoundBuffer() const final {
            throw exception("Not implemented");
        }

        [[nodiscard]] u32 LocalMemorySize() const final {
            return static_cast<u32>(sph.LocalMemorySize()) + sph.common3.shader_local_memory_crs_size;
        }

        [[nodiscard]] u32 SharedMemorySize() const final {
            return 0; // Only relevant for compute shaders
        }

        [[nodiscard]] std::array<u32, 3> WorkgroupSize() const final {
            return {0, 0, 0}; // Only relevant for compute shaders
        }

        void Dump(u64 hash) final {}
    };

    Shader::IR::Program ShaderManager::ParseGraphicsShader(const std::array<u32, 8> &postVtgShaderAttributeSkipMask,
                                                           Shader::Stage stage,
                                                           span<u8> binary, u32 baseOffset,
                                                           u32 textureConstantBufferIndex,
                                                           bool viewportTransformEnabled,
                                                           const ConstantBufferRead &constantBufferRead, const GetTextureType &getTextureType) {
        std::scoped_lock lock{poolMutex};

        GraphicsEnvironment environment{postVtgShaderAttributeSkipMask, stage, binary, baseOffset, textureConstantBufferIndex, viewportTransformEnabled, constantBufferRead, getTextureType};
        Shader::Maxwell::Flow::CFG cfg{environment, flowBlockPool, Shader::Maxwell::Location{static_cast<u32>(baseOffset + sizeof(Shader::ProgramHeader))}};
        return  Shader::Maxwell::TranslateProgram(instructionPool, blockPool, environment, cfg, hostTranslateInfo);
    }

    Shader::IR::Program ShaderManager::CombineVertexShaders(Shader::IR::Program &vertexA, Shader::IR::Program &vertexB, span<u8> vertexBBinary) {
        std::scoped_lock lock{poolMutex};

        VertexBEnvironment env{vertexBBinary};
        return Shader::Maxwell::MergeDualVertexPrograms(vertexA, vertexB, env);
    }

    Shader::IR::Program ShaderManager::ParseComputeShader(span<u8> binary, u32 baseOffset,
                                                          u32 textureConstantBufferIndex,
                                                          u32 localMemorySize, u32 sharedMemorySize,
                                                          std::array<u32, 3> workgroupDimensions,
                                                          const ConstantBufferRead &constantBufferRead, const GetTextureType &getTextureType) {
        std::scoped_lock lock{poolMutex};

        ComputeEnvironment environment{binary, baseOffset, textureConstantBufferIndex, localMemorySize, sharedMemorySize, workgroupDimensions, constantBufferRead, getTextureType};
        Shader::Maxwell::Flow::CFG cfg{environment, flowBlockPool, Shader::Maxwell::Location{static_cast<u32>(baseOffset)}};
        return  Shader::Maxwell::TranslateProgram(instructionPool, blockPool, environment, cfg, hostTranslateInfo);
    }


    vk::ShaderModule ShaderManager::CompileShader(const Shader::RuntimeInfo &runtimeInfo, Shader::IR::Program &program, Shader::Backend::Bindings &bindings) {
        std::scoped_lock lock{poolMutex};

        if (program.info.loads.Legacy() || program.info.stores.Legacy())
            Shader::Maxwell::ConvertLegacyToGeneric(program, runtimeInfo);

        auto spirv{Shader::Backend::SPIRV::EmitSPIRV(profile, runtimeInfo, program, bindings)};

        vk::ShaderModuleCreateInfo createInfo{
            .pCode = spirv.data(),
            .codeSize = spirv.size() * sizeof(u32),
        };

        return (*gpu.vkDevice).createShaderModule(createInfo, nullptr, *gpu.vkDevice.getDispatcher());
    }

    void ShaderManager::ResetPools() {
        std::scoped_lock lock{poolMutex};

        instructionPool.ReleaseContents();
        blockPool.ReleaseContents();
        flowBlockPool.ReleaseContents();
    }
}
