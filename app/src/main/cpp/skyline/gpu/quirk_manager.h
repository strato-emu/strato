// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vulkan/vulkan.hpp>
#include <common.h>

namespace skyline::gpu {
    /**
     * @brief Checks and stores all the quirks of the host GPU discovered at runtime
     */
    class QuirkManager {
      public:
        bool supportsLastProvokingVertex{}; //!< If the device supports setting the last vertex as the provoking vertex (with VK_EXT_provoking_vertex)
        bool supportsLogicOp{}; //!< If the device supports framebuffer logical operations during blending
        bool supportsVertexAttributeDivisor{}; //!< If the device supports a divisor for instance-rate vertex attributes (with VK_EXT_vertex_attribute_divisor)
        bool supportsVertexAttributeZeroDivisor{}; //!< If the device supports a zero divisor for instance-rate vertex attributes (with VK_EXT_vertex_attribute_divisor)
        bool supportsMultipleViewports{}; //!< If the device supports more than one viewport
        bool supportsSpirv14{}; //!< If SPIR-V 1.4 is supported (with VK_KHR_spirv_1_4)
        bool supportsFloat16{}; //!< If 16-bit floating point integers are supported in shaders
        bool supportsInt8{}; //!< If 8-bit integers are supported in shaders
        bool supportsInt16{}; //!< If 16-bit integers are supported in shaders
        bool supportsInt64{}; //!< If 64-bit integers are supported in shaders
        bool supportsAtomicInt64{}; //!< If atomic operations on 64-bit integers are supported in shaders
        bool supportsFloatControls{}; //!< If extensive control over FP behavior is exposed (with VK_KHR_shader_float_controls)
        bool supportsImageReadWithoutFormat{}; //!< If a storage image can be read without a format

        QuirkManager() = default;

        using DeviceFeatures2 = vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVertexAttributeDivisorFeaturesEXT, vk::PhysicalDeviceShaderFloat16Int8Features, vk::PhysicalDeviceShaderAtomicInt64Features>;

        QuirkManager(const vk::PhysicalDeviceProperties &properties, const DeviceFeatures2 &deviceFeatures2, DeviceFeatures2 &enabledFeatures2, const std::vector<vk::ExtensionProperties> &deviceExtensions, std::vector<std::array<char, VK_MAX_EXTENSION_NAME_SIZE>> &enabledExtensions);

        /**
         * @return A summary of all the GPU quirks as a human-readable string
         */
        std::string Summary();
    };
}
