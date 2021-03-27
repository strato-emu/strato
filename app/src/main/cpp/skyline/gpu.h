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
        vk::raii::Context context;
        vk::raii::Instance instance;
        vk::raii::DebugReportCallbackEXT debugReportCallback;
        vk::Device device;

        static vk::raii::Instance CreateInstance(const DeviceState &state, const vk::raii::Context &context);

        static vk::raii::DebugReportCallbackEXT CreateDebugReportCallback(const DeviceState &state, const vk::raii::Instance &instance);

        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char *layerPrefix, const char *message, Logger *logger);

      public:
        PresentationEngine presentation;

        GPU(const DeviceState &state);
    };
}
