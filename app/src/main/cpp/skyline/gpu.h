// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "gpu/presentation_engine.h"

namespace skyline::gpu {
    /**
     * @brief An interface to host GPU structures, anything concerning host GPU/Presentation APIs is encapsulated by this
     */
    class GPU {
      private:
        static vk::raii::Instance CreateInstance(const DeviceState &state, const vk::raii::Context &context);

        static vk::raii::DebugReportCallbackEXT CreateDebugReportCallback(const DeviceState &state, const vk::raii::Instance &instance);

        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char *layerPrefix, const char *message, Logger *logger);

        static vk::raii::PhysicalDevice CreatePhysicalDevice(const DeviceState &state, const vk::raii::Instance &instance);

        static vk::raii::Device CreateDevice(const DeviceState &state, const vk::raii::PhysicalDevice &physicalDevice);

      public:
        vk::raii::Context vkContext; //!< An overarching context for Vulkan with
        vk::raii::Instance vkInstance; //!< An instance of Vulkan with all application context
        vk::raii::DebugReportCallbackEXT vkDebugReportCallback; //!< An RAII Vulkan debug report manager which calls into DebugCallback
        vk::raii::PhysicalDevice vkPhysicalDevice; //!< The underlying physical Vulkan device from which we derieve our logical device
        vk::raii::Device vkDevice; //!< The logical Vulkan device which we want to render using
        vk::raii::Queue vkQueue; //!< A Vulkan Queue supporting graphics and compute operations

        PresentationEngine presentation;

        GPU(const DeviceState &state);
    };
}
