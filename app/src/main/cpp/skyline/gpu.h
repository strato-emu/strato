// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "gpu/quirk_manager.h"
#include "gpu/memory_manager.h"
#include "gpu/command_scheduler.h"
#include "gpu/presentation_engine.h"
#include "gpu/texture_manager.h"
#include "gpu/shader_manager.h"

namespace skyline::gpu {
    /**
     * @brief An interface to host GPU structures, anything concerning host GPU/Presentation APIs is encapsulated by this
     */
    class GPU {
      private:
        static vk::raii::Instance CreateInstance(const DeviceState &state, const vk::raii::Context &context);

        static vk::raii::DebugReportCallbackEXT CreateDebugReportCallback(const vk::raii::Instance &instance);

        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char *layerPrefix, const char *message);

        static vk::raii::PhysicalDevice CreatePhysicalDevice(const vk::raii::Instance &instance);

        static vk::raii::Device CreateDevice(const vk::raii::PhysicalDevice &physicalDevice, decltype(vk::DeviceQueueCreateInfo::queueCount)& queueConfiguration, QuirkManager& quirks);

      public:
        static constexpr u32 VkApiVersion{VK_API_VERSION_1_1}; //!< The version of core Vulkan that we require

        vk::raii::Context vkContext;
        vk::raii::Instance vkInstance;
        vk::raii::DebugReportCallbackEXT vkDebugReportCallback; //!< An RAII Vulkan debug report manager which calls into 'GPU::DebugCallback'
        vk::raii::PhysicalDevice vkPhysicalDevice;
        u32 vkQueueFamilyIndex{};
        vk::raii::Device vkDevice;
        std::mutex queueMutex; //!< Synchronizes access to the queue as it is externally synchronized
        vk::raii::Queue vkQueue; //!< A Vulkan Queue supporting graphics and compute operations

        QuirkManager quirks;

        memory::MemoryManager memory;
        CommandScheduler scheduler;
        PresentationEngine presentation;

        TextureManager texture;

        ShaderManager shader;

        GPU(const DeviceState &state);
    };
}
