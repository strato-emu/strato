// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <shader_compiler/common/settings.h>
#include <shader_compiler/common/log.h>
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
        auto& quirks{gpu.quirks};
        hostTranslateInfo = Shader::HostTranslateInfo{
            .support_float16 = quirks.supportsFloat16,
            .support_int64 = quirks.supportsInt64,
            .needs_demote_reorder = false,
        };

        profile = Shader::Profile{
            .supported_spirv = quirks.supportsSpirv14 ? 0x00010400U : 0x00010000U,
            .unified_descriptor_binding = true,
            .support_descriptor_aliasing = true,
            .support_int8 = quirks.supportsInt8,
            .support_int16 = quirks.supportsInt16,
            .support_int64 = quirks.supportsInt64,
            .support_vertex_instance_id = false,
            .support_float_controls = quirks.supportsFloatControls,
            // TODO: Float control specifics
            .support_vote = true,
            .support_viewport_index_layer_non_geometry = false,
            .support_viewport_mask = false,
            .support_typeless_image_loads = quirks.supportsImageReadWithoutFormat,
            .support_demote_to_helper_invocation = true,
            .support_int64_atomics = false,
            .support_derivative_control = true,
            .support_geometry_shader_passthrough = false,
            // TODO: Warp size property
            .lower_left_origin_mode = false,
            .need_declared_frag_colors = false,
        };

        Shader::Settings::values = {
            .disable_shader_loop_safety_checks = false,
            .renderer_debug = true,
            .resolution_info = {
                .active = false,
            },
        };
    }
}
