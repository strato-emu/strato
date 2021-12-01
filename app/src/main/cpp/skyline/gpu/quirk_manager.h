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

        QuirkManager() = default;

        using DeviceFeatures2 = vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVertexAttributeDivisorFeaturesEXT>;

        QuirkManager(const vk::PhysicalDeviceProperties &properties, const DeviceFeatures2 &deviceFeatures2, DeviceFeatures2 &enabledFeatures2, const std::vector<vk::ExtensionProperties> &deviceExtensions, std::vector<std::array<char, VK_MAX_EXTENSION_NAME_SIZE>> &enabledExtensions);

        /**
         * @return A summary of all the GPU quirks as a human-readable string
         */
        std::string Summary();
    };
}
