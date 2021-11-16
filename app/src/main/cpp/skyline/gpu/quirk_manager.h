// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vulkan/vulkan.hpp>
#include <common.h>

namespace skyline {
    /**
     * @brief Checks and stores all the quirks of the host GPU discovered at runtime
     */
    class QuirkManager {
      public:
        bool supportsLastProvokingVertex{}; //!< If the device supports setting the last vertex as the provoking vertex (with VK_EXT_provoking_vertex)
        bool supportsLogicOp{}; //!< If the device supports framebuffer logical operations during blending
        bool supportsVertexAttributeDivisor{}; //!< If the device supports a divisor for instance-rate vertex attributes (with VK_EXT_vertex_attribute_divisor)

        QuirkManager() = default;

        QuirkManager(const vk::PhysicalDeviceProperties &properties, const vk::PhysicalDeviceFeatures2 &deviceFeatures2, vk::PhysicalDeviceFeatures2 &enabledFeatures2, const std::vector<vk::ExtensionProperties> &deviceExtensions, std::vector<std::array<char, VK_MAX_EXTENSION_NAME_SIZE>> &enabledExtensions);

        /**
         * @return A summary of all the GPU quirks as a human-readable string
         */
        std::string Summary();
    };
}
