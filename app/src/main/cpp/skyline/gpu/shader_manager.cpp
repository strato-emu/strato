// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <shader_compiler/common/settings.h>
#include <shader_compiler/common/log.h>
#include <shader_compiler/frontend/maxwell/translate_program.h>
#include <shader_compiler/backend/spirv/emit_spirv.h>
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
    ShaderManager::ShaderManager(const DeviceState &state, GPU &gpu) : gpu(gpu) {
        auto &quirks{gpu.quirks};
        hostTranslateInfo = Shader::HostTranslateInfo{
            .support_float16 = quirks.supportsFloat16,
            .support_int64 = quirks.supportsInt64,
            .needs_demote_reorder = false,
        };

        constexpr u32 TegraX1WarpSize{32}; //!< The amount of threads in a warp on the Tegra X1
        profile = Shader::Profile{
            .supported_spirv = quirks.supportsSpirv14 ? 0x00010400U : 0x00010000U,
            .unified_descriptor_binding = true,
            .support_descriptor_aliasing = true,
            .support_int8 = quirks.supportsInt8,
            .support_int16 = quirks.supportsInt16,
            .support_int64 = quirks.supportsInt64,
            .support_vertex_instance_id = false,
            .support_float_controls = quirks.supportsFloatControls,
            .support_separate_denorm_behavior = quirks.floatControls.denormBehaviorIndependence == vk::ShaderFloatControlsIndependence::eAll,
            .support_separate_rounding_mode = quirks.floatControls.roundingModeIndependence == vk::ShaderFloatControlsIndependence::eAll,
            .support_fp16_denorm_preserve = static_cast<bool>(quirks.floatControls.shaderDenormPreserveFloat16),
            .support_fp32_denorm_preserve = static_cast<bool>(quirks.floatControls.shaderDenormPreserveFloat32),
            .support_fp16_denorm_flush = static_cast<bool>(quirks.floatControls.shaderDenormFlushToZeroFloat16),
            .support_fp32_denorm_flush = static_cast<bool>(quirks.floatControls.shaderDenormFlushToZeroFloat32),
            .support_fp16_signed_zero_nan_preserve = static_cast<bool>(quirks.floatControls.shaderSignedZeroInfNanPreserveFloat16),
            .support_fp32_signed_zero_nan_preserve = static_cast<bool>(quirks.floatControls.shaderSignedZeroInfNanPreserveFloat32),
            .support_fp64_signed_zero_nan_preserve = static_cast<bool>(quirks.floatControls.shaderSignedZeroInfNanPreserveFloat64),
            .support_explicit_workgroup_layout = false,
            .support_vote = quirks.supportsSubgroupVote,
            .support_viewport_index_layer_non_geometry = quirks.supportsShaderViewportIndexLayer,
            .support_viewport_mask = false,
            .support_typeless_image_loads = quirks.supportsImageReadWithoutFormat,
            .support_demote_to_helper_invocation = true,
            .support_int64_atomics = quirks.supportsAtomicInt64,
            .support_derivative_control = true,
            .support_geometry_shader_passthrough = false,
            .warp_size_potentially_larger_than_guest = TegraX1WarpSize < quirks.subgroupSize,
            .lower_left_origin_mode = false,
            .need_declared_frag_colors = false,
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
        std::vector<u8> binary;
        u32 baseOffset;

      public:
        explicit GraphicsEnvironment(std::vector<u8> pBinary, u32 baseOffset, Shader::Stage pStage) : binary(std::move(pBinary)), baseOffset(baseOffset) {
            sph = *reinterpret_cast<Shader::ProgramHeader *>(binary.data());
            start_address = baseOffset;
            stage = pStage;
        }

        [[nodiscard]] u64 ReadInstruction(u32 address) final {
            address -= baseOffset;
            if (binary.size() < (address + sizeof(u64)))
                throw exception("Out of bounds instruction read: 0x{:X}", address);
            return *reinterpret_cast<u64 *>(binary.data() + address);
        }

        [[nodiscard]] u32 ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) final {
            throw exception("Not implemented");
        }

        [[nodiscard]] Shader::TextureType ReadTextureType(u32 raw_handle) final {
            throw exception("Not implemented");
        }

        [[nodiscard]] u32 TextureBoundBuffer() const final {
            throw exception("Not implemented");
        }

        [[nodiscard]] u32 LocalMemorySize() const final {
            return static_cast<u32>(sph.LocalMemorySize()) + sph.common3.shader_local_memory_crs_size;
        }

        [[nodiscard]] u32 SharedMemorySize() const final {
            return 0; // Shared memory size is only relevant for compute shaders
        }

        [[nodiscard]] std::array<u32, 3> WorkgroupSize() const final {
            return {0, 0, 0}; // Workgroup size is only relevant for compute shaders
        }
    };

    vk::raii::ShaderModule ShaderManager::CompileGraphicsShader(const std::vector<u8> &binary, Shader::Stage stage, u32 baseOffset, Shader::RuntimeInfo &runtimeInfo, Shader::Backend::Bindings &bindings) {
        GraphicsEnvironment environment{binary, baseOffset, stage};
        Shader::Maxwell::Flow::CFG cfg(environment, flowBlockPool, Shader::Maxwell::Location{static_cast<u32>(baseOffset + sizeof(Shader::ProgramHeader))});

        auto program{Shader::Maxwell::TranslateProgram(instPool, blockPool, environment, cfg, hostTranslateInfo)};

        auto spirv{Shader::Backend::SPIRV::EmitSPIRV(profile, runtimeInfo, program, bindings)};

        runtimeInfo.previous_stage_stores = program.info.stores;
        if (program.is_geometry_passthrough)
            runtimeInfo.previous_stage_stores.mask |= program.info.passthrough.mask;

        vk::ShaderModuleCreateInfo createInfo{
            .pCode = spirv.data(),
            .codeSize = spirv.size() * sizeof(u32),
        };
        vk::raii::ShaderModule shaderModule(gpu.vkDevice, createInfo);

        return shaderModule;
    }
}
