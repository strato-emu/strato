// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <range/v3/algorithm.hpp>
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
    ShaderManager::ShaderManager(const DeviceState &state, GPU &gpu) : gpu{gpu} {
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
        ShaderManager::ConstantBufferRead constantBufferRead;
        ShaderManager::GetTextureType getTextureType;

      public:
        std::vector<ShaderManager::ConstantBufferWord> constantBufferWords;
        std::vector<ShaderManager::CachedTextureType> textureTypes;

        GraphicsEnvironment(Shader::Stage pStage, span<u8> pBinary, u32 baseOffset, u32 textureBufferIndex, ShaderManager::ConstantBufferRead constantBufferRead, ShaderManager::GetTextureType getTextureType) : binary{pBinary}, baseOffset{baseOffset}, textureBufferIndex{textureBufferIndex}, constantBufferRead{std::move(constantBufferRead)}, getTextureType{std::move(getTextureType)} {
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
            auto value{constantBufferRead(index, offset)};
            constantBufferWords.emplace_back(index, offset, value);
            return value;
        }

        [[nodiscard]] Shader::TextureType ReadTextureType(u32 handle) final {
            auto type{getTextureType(handle)};
            textureTypes.emplace_back(handle, type);
            return type;
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

    constexpr ShaderManager::ConstantBufferWord::ConstantBufferWord(u32 index, u32 offset, u32 value) : index(index), offset(offset), value(value) {}

    constexpr ShaderManager::CachedTextureType::CachedTextureType(u32 handle, Shader::TextureType type) : handle(handle), type(type) {}

    ShaderManager::ShaderProgram::ShaderProgram(Shader::IR::Program &&program) : program{std::move(program)} {}

    bool ShaderManager::SingleShaderProgram::VerifyState(const ConstantBufferRead &constantBufferRead, const GetTextureType &getTextureType) const {
        return ranges::all_of(constantBufferWords, [&](const ConstantBufferWord &word) {
            return constantBufferRead(word.index, word.offset) == word.value;
        }) &&
            ranges::all_of(textureTypes, [&](const CachedTextureType &type) {
                return getTextureType(type.handle) == type.type;
            });
    }

    ShaderManager::GuestShaderKey::GuestShaderKey(Shader::Stage stage, span<u8> bytecode, u32 textureConstantBufferIndex, span<ConstantBufferWord> constantBufferWords, span<CachedTextureType> textureTypes) : stage{stage}, bytecode{bytecode.begin(), bytecode.end()}, textureConstantBufferIndex{textureConstantBufferIndex}, constantBufferWords{constantBufferWords}, textureTypes{textureTypes} {}

    ShaderManager::GuestShaderLookup::GuestShaderLookup(Shader::Stage stage, span<u8> bytecode, u32 textureConstantBufferIndex, ConstantBufferRead constantBufferRead, GetTextureType getTextureType) : stage(stage), textureConstantBufferIndex(textureConstantBufferIndex), bytecode(bytecode), constantBufferRead(std::move(constantBufferRead)), getTextureType(std::move(getTextureType)) {}

    #define HASH(member) boost::hash_combine(hash, key.member)

    size_t ShaderManager::ShaderProgramHash::operator()(const GuestShaderKey &key) const noexcept {
        size_t hash{};

        HASH(stage);
        hash = XXH64(key.bytecode.data(), key.bytecode.size(), hash);
        HASH(textureConstantBufferIndex);

        return hash;
    }

    size_t ShaderManager::ShaderProgramHash::operator()(const GuestShaderLookup &key) const noexcept {
        size_t hash{};

        HASH(stage);
        hash = XXH64(key.bytecode.data(), key.bytecode.size(), hash);
        HASH(textureConstantBufferIndex);

        return hash;
    }

    #undef HASH

    bool ShaderManager::ShaderProgramEqual::operator()(const GuestShaderKey &lhs, const GuestShaderLookup &rhs) const noexcept {
        return lhs.stage == rhs.stage &&
            ranges::equal(lhs.bytecode, rhs.bytecode) &&
            lhs.textureConstantBufferIndex == rhs.textureConstantBufferIndex &&
            ranges::all_of(lhs.constantBufferWords, [&constantBufferRead = rhs.constantBufferRead](const ConstantBufferWord &word) {
                return constantBufferRead(word.index, word.offset) == word.value;
            }) &&
            ranges::all_of(lhs.textureTypes, [&getTextureType = rhs.getTextureType](const CachedTextureType &type) {
                return getTextureType(type.handle) == type.type;
            });
    }

    bool ShaderManager::ShaderProgramEqual::operator()(const GuestShaderKey &lhs, const GuestShaderKey &rhs) const noexcept {
        return lhs.stage == rhs.stage &&
            ranges::equal(lhs.bytecode, rhs.bytecode) &&
            lhs.textureConstantBufferIndex == rhs.textureConstantBufferIndex &&
            ranges::equal(lhs.constantBufferWords, rhs.constantBufferWords) &&
            ranges::equal(lhs.textureTypes, rhs.textureTypes);
    }

    ShaderManager::DualVertexShaderProgram::DualVertexShaderProgram(Shader::IR::Program ir, std::shared_ptr<ShaderProgram> vertexA, std::shared_ptr<ShaderProgram> vertexB) : ShaderProgram{std::move(ir)}, vertexA{std::move(vertexA)}, vertexB{std::move(vertexB)} {}

    bool ShaderManager::DualVertexShaderProgram::VerifyState(const ConstantBufferRead &constantBufferRead, const GetTextureType& getTextureType) const {
        return vertexA->VerifyState(constantBufferRead, getTextureType) && vertexB->VerifyState(constantBufferRead, getTextureType);
    }

    std::shared_ptr<ShaderManager::ShaderProgram> ShaderManager::ParseGraphicsShader(Shader::Stage stage, span<u8> binary, u32 baseOffset, u32 textureConstantBufferIndex, const ConstantBufferRead &constantBufferRead, const GetTextureType &getTextureType) {
        std::unique_lock lock{programMutex};

        auto it{programCache.find(GuestShaderLookup{stage, binary, textureConstantBufferIndex, constantBufferRead, getTextureType})};
        if (it != programCache.end())
            return it->second;

        lock.unlock();

        auto program{std::make_shared<SingleShaderProgram>()};
        GraphicsEnvironment environment{stage, binary, baseOffset, textureConstantBufferIndex, constantBufferRead, getTextureType};
        Shader::Maxwell::Flow::CFG cfg(environment, program->flowBlockPool, Shader::Maxwell::Location{static_cast<u32>(baseOffset + sizeof(Shader::ProgramHeader))});
        program->program = Shader::Maxwell::TranslateProgram(program->instructionPool, program->blockPool, environment, cfg, hostTranslateInfo);
        program->constantBufferWords = std::move(environment.constantBufferWords);
        program->textureTypes = std::move(environment.textureTypes);

        lock.lock();

        auto programIt{programCache.try_emplace(GuestShaderKey{stage, binary, textureConstantBufferIndex, program->constantBufferWords, program->textureTypes}, std::move(program))};
        return programIt.first->second;
    }

    constexpr size_t ShaderManager::DualVertexProgramsHash::operator()(const std::pair<std::shared_ptr<ShaderProgram>, std::shared_ptr<ShaderProgram>> &p) const {
        size_t hash{};
        boost::hash_combine(hash, p.first);
        boost::hash_combine(hash, p.second);
        return hash;
    }

    std::shared_ptr<ShaderManager::ShaderProgram> ShaderManager::CombineVertexShaders(const std::shared_ptr<ShaderProgram> &vertexA, const std::shared_ptr<ShaderProgram> &vertexB, span<u8> vertexBBinary) {
        std::unique_lock lock{programMutex};
        auto &program{dualProgramCache[DualVertexPrograms{vertexA, vertexB}]};
        if (program)
            return program;

        lock.unlock();

        VertexBEnvironment vertexBEnvironment{vertexBBinary};
        auto mergedProgram{std::make_shared<DualVertexShaderProgram>(Shader::Maxwell::MergeDualVertexPrograms(vertexA->program, vertexB->program, vertexBEnvironment), vertexA, vertexB)};

        lock.lock();

        return program = std::move(mergedProgram);
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

    constexpr size_t ShaderManager::ShaderModuleStateHash::operator()(const ShaderModuleState &state) const {
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

    ShaderManager::ShaderModule::ShaderModule(const vk::raii::Device &device, const vk::ShaderModuleCreateInfo &createInfo, Shader::Backend::Bindings bindings) : vkModule{device, createInfo}, bindings{bindings} {}

    vk::ShaderModule ShaderManager::CompileShader(Shader::RuntimeInfo &runtimeInfo, const std::shared_ptr<ShaderProgram> &program, Shader::Backend::Bindings &bindings) {
        std::unique_lock lock{moduleMutex};
        ShaderModuleState shaderModuleState{program, bindings, runtimeInfo};
        auto it{shaderModuleCache.find(shaderModuleState)};
        if (it != shaderModuleCache.end()) {
            const auto &entry{it->second};
            bindings = entry.bindings;
            return *entry.vkModule;
        }

        lock.unlock();

        // Note: EmitSPIRV will change bindings so we explicitly have pre/post emit bindings
        if (program->program.info.loads.Legacy() || program->program.info.stores.Legacy()) {
            // The legacy conversion pass modifies the underlying program based on runtime state, so without making a copy of the program there may be issues if runtimeInfo changes
            Logger::Warn("Shader uses legacy attributes, beware!");
            Shader::Maxwell::ConvertLegacyToGeneric(program->program, runtimeInfo);
        }
        auto spirv{Shader::Backend::SPIRV::EmitSPIRV(profile, runtimeInfo, program->program, bindings)};

        vk::ShaderModuleCreateInfo createInfo{
            .pCode = spirv.data(),
            .codeSize = spirv.size() * sizeof(u32),
        };

        lock.lock();

        auto shaderModule{shaderModuleCache.try_emplace(shaderModuleState, gpu.vkDevice, createInfo, bindings)};
        return *(shaderModule.first->second.vkModule);
    }
}
