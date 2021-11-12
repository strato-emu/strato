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

        QuirkManager() = default;

        QuirkManager(vk::PhysicalDeviceProperties properties, vk::PhysicalDeviceFeatures2 features, const std::vector<vk::ExtensionProperties>& extensions);
    };
}
