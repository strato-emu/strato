// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <boost/functional/hash.hpp>
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
        auto &traits{gpu.traits};
        hostTranslateInfo = Shader::HostTranslateInfo{
            .support_float16 = traits.supportsFloat16,
            .support_int64 = traits.supportsInt64,
            .needs_demote_reorder = false,
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

      public:
        GraphicsEnvironment(Shader::Stage pStage, span<u8> pBinary, u32 baseOffset, u32 textureBufferIndex) : binary(pBinary), baseOffset(baseOffset), textureBufferIndex(textureBufferIndex) {
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

        [[nodiscard]] u32 ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) final {
            throw exception("Not implemented");
        }

        [[nodiscard]] Shader::TextureType ReadTextureType(u32 raw_handle) final {
            throw exception("Not implemented");
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
            return 0; // Only relevant for compute shaders
        }

        [[nodiscard]] std::array<u32, 3> WorkgroupSize() const final {
            return {0, 0, 0}; // Only relevant for compute shaders
        }
    };

    ShaderManager::DualVertexShaderProgram::DualVertexShaderProgram(Shader::IR::Program ir, std::shared_ptr<ShaderProgram> vertexA, std::shared_ptr<ShaderProgram> vertexB) : ShaderProgram{std::move(ir)}, vertexA(std::move(vertexA)), vertexB(std::move(vertexB)) {}

    std::shared_ptr<ShaderManager::ShaderProgram> ShaderManager::ParseGraphicsShader(Shader::Stage stage, span<u8> binary, u32 baseOffset, u32 bindlessTextureConstantBufferIndex) {
        auto &program{programCache[binary]};
        if (program)
            return program;

        program = std::make_shared<SingleShaderProgram>();
        GraphicsEnvironment environment{stage, binary, baseOffset, bindlessTextureConstantBufferIndex};
        Shader::Maxwell::Flow::CFG cfg(environment, program->flowBlockPool, Shader::Maxwell::Location{static_cast<u32>(baseOffset + sizeof(Shader::ProgramHeader))});
        program->program = Shader::Maxwell::TranslateProgram(program->instructionPool, program->blockPool, environment, cfg, hostTranslateInfo);
        return program;
    }

    constexpr size_t ShaderManager::DualVertexProgramsHash::operator()(const std::pair<std::shared_ptr<ShaderProgram>, std::shared_ptr<ShaderProgram>> &p) const {
        size_t hash{};
        boost::hash_combine(hash, p.first);
        boost::hash_combine(hash, p.second);
        return hash;
    }

    std::shared_ptr<ShaderManager::ShaderProgram> ShaderManager::CombineVertexShaders(const std::shared_ptr<ShaderManager::ShaderProgram> &vertexA, const std::shared_ptr<ShaderManager::ShaderProgram> &vertexB, span<u8> vertexBBinary) {
        auto &program{dualProgramCache[DualVertexPrograms{vertexA, vertexB}]};
        if (program)
            return program;

        VertexBEnvironment vertexBEnvironment{vertexBBinary};
        program = std::make_shared<DualVertexShaderProgram>(Shader::Maxwell::MergeDualVertexPrograms(vertexA->program, vertexB->program, vertexBEnvironment), vertexA, vertexB);
        return program;
    }

    bool ShaderManager::ShaderModuleState::operator==(const ShaderModuleState &other) const {
        if (program != other.program)
            return false;

        if (bindings.unified != other.bindings.unified || bindings.uniform_buffer != other.bindings.uniform_buffer || bindings.storage_buffer != other.bindings.storage_buffer || bindings.texture != other.bindings.texture || bindings.image != other.bindings.image || bindings.texture_scaling_index != other.bindings.texture_scaling_index || bindings.image_scaling_index != other.bindings.image_scaling_index)
            return false;

        static_assert(sizeof(Shader::Backend::Bindings) == 0x1C);

        if (!std::equal(runtimeInfo.generic_input_types.begin(), runtimeInfo.generic_input_types.end(), other.runtimeInfo.generic_input_types.begin()))
            return false;

        #define NEQ(member) runtimeInfo.member != other.runtimeInfo.member

        if (NEQ(previous_stage_stores.mask) || NEQ(convert_depth_mode) || NEQ(force_early_z) || NEQ(tess_primitive) || NEQ(tess_spacing) || NEQ(tess_clockwise) || NEQ(input_topology) || NEQ(fixed_state_point_size) || NEQ(alpha_test_func) || NEQ(alpha_test_reference) || NEQ(y_negate) || NEQ(glasm_use_storage_buffers))
            return false;

        #undef NEQ

        if (!std::equal(runtimeInfo.xfb_varyings.begin(), runtimeInfo.xfb_varyings.end(), other.runtimeInfo.xfb_varyings.begin(), [](const Shader::TransformFeedbackVarying &a, const Shader::TransformFeedbackVarying &b) {
            return a.buffer == b.buffer && a.stride == b.stride && a.offset == b.offset && a.components == b.components;
        }))
            return false;

        static_assert(sizeof(Shader::RuntimeInfo) == 0x88);

        return true;
    }

    constexpr size_t ShaderManager::ShaderModuleStateHash::operator()(const ShaderManager::ShaderModuleState &state) const {
        size_t hash{};

        boost::hash_combine(hash, state.program);

        hash = XXH64(&state.bindings, sizeof(Shader::Backend::Bindings), hash);

        #define RIH(member) boost::hash_combine(hash, state.runtimeInfo.member)

        hash = XXH64(state.runtimeInfo.generic_input_types.data(), state.runtimeInfo.generic_input_types.size() * sizeof(Shader::AttributeType), hash);
        hash = XXH64(&state.runtimeInfo.previous_stage_stores.mask, sizeof(state.runtimeInfo.previous_stage_stores.mask), hash);
        RIH(convert_depth_mode);
        RIH(force_early_z);
        RIH(tess_primitive);
        RIH(tess_spacing);
        RIH(tess_clockwise);
        RIH(input_topology);
        RIH(fixed_state_point_size.value_or(NAN));
        RIH(alpha_test_func.value_or(Shader::CompareFunction::Never));
        RIH(alpha_test_reference);
        RIH(glasm_use_storage_buffers);
        hash = XXH64(state.runtimeInfo.xfb_varyings.data(), state.runtimeInfo.xfb_varyings.size() * sizeof(Shader::TransformFeedbackVarying), hash);

        static_assert(sizeof(Shader::RuntimeInfo) == 0x88);
        #undef RIH

        return hash;
    }

    ShaderManager::ShaderModule::ShaderModule(const vk::raii::Device &device, const vk::ShaderModuleCreateInfo &createInfo, Shader::Backend::Bindings bindings) : vkModule(device, createInfo), bindings(bindings) {}

    vk::ShaderModule ShaderManager::CompileShader(Shader::RuntimeInfo &runtimeInfo, const std::shared_ptr<ShaderProgram> &program, Shader::Backend::Bindings &bindings) {
        ShaderModuleState shaderModuleState{program, bindings, runtimeInfo};
        auto it{shaderModuleCache.find(shaderModuleState)};
        if (it != shaderModuleCache.end()) {
            const auto &entry{it->second};
            bindings = entry.bindings;
            return *entry.vkModule;
        }

        // Note: EmitSPIRV will change bindings so we explicitly have pre/post emit bindings
        auto spirv{Shader::Backend::SPIRV::EmitSPIRV(profile, runtimeInfo, program->program, bindings)};

        vk::ShaderModuleCreateInfo createInfo{
            .pCode = spirv.data(),
            .codeSize = spirv.size() * sizeof(u32),
        };

        auto shaderModule{shaderModuleCache.try_emplace(shaderModuleState, gpu.vkDevice, createInfo, bindings)};
        return *(shaderModule.first->second.vkModule);
    }
}
