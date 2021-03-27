// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "gpu.h"

namespace skyline::gpu {
    vk::raii::Instance GPU::CreateInstance(const DeviceState &state, const vk::raii::Context &context) {
        vk::ApplicationInfo applicationInfo{
            .pApplicationName = "Skyline",
            .applicationVersion = VK_MAKE_VERSION('S', 'K', 'Y'), // "SKY" magic as the application version
            .pEngineName = "GPU",
            .engineVersion = VK_MAKE_VERSION('G', 'P', 'U'), // "GPU" magic as engine version
            .apiVersion = VK_API_VERSION_1_1,
        };

        #ifdef NDEBUG
        std::array<const char *, 0> requiredLayers{};
        #else
        std::array<const char *, 1> requiredLayers{
            "VK_LAYER_KHRONOS_validation"
        };
        #endif

        auto instanceLayers{context.enumerateInstanceLayerProperties()};
        if (state.logger->configLevel >= Logger::LogLevel::Debug) {
            std::string layers;
            for (const auto &instanceLayer : instanceLayers)
                layers += fmt::format("\n* {} (Sv{}.{}.{}, Iv{}.{}.{}) - {}", instanceLayer.layerName, VK_VERSION_MAJOR(instanceLayer.specVersion), VK_VERSION_MINOR(instanceLayer.specVersion), VK_VERSION_PATCH(instanceLayer.specVersion), VK_VERSION_MAJOR(instanceLayer.implementationVersion), VK_VERSION_MINOR(instanceLayer.implementationVersion), VK_VERSION_PATCH(instanceLayer.implementationVersion), instanceLayer.description);
            state.logger->Debug("Vulkan Layers:{}", layers);
        }

        for (const auto &requiredLayer : requiredLayers) {
            if (![&] {
                for (const auto &instanceLayer : instanceLayers)
                    if (std::string_view(instanceLayer.layerName) == std::string_view(requiredLayer))
                        return true;
                return false;
            }())
                throw exception("Cannot find Vulkan layer: \"{}\"", requiredLayer);
        }

        #ifdef NDEBUG
        std::array<const char*, 0> requiredInstanceExtensions{};
        #else
        std::array<const char *, 1> requiredInstanceExtensions{
            VK_EXT_DEBUG_REPORT_EXTENSION_NAME
        };
        #endif

        auto instanceExtensions{context.enumerateInstanceExtensionProperties()};
        if (state.logger->configLevel >= Logger::LogLevel::Debug) {
            std::string extensions;
            for (const auto &instanceExtension : instanceExtensions)
                extensions += fmt::format("\n* {} (v{}.{}.{})", instanceExtension.extensionName, VK_VERSION_MAJOR(instanceExtension.specVersion), VK_VERSION_MINOR(instanceExtension.specVersion), VK_VERSION_PATCH(instanceExtension.specVersion));
            state.logger->Debug("Vulkan Instance Extensions:{}", extensions);
        }

        for (const auto &requiredExtension : requiredInstanceExtensions) {
            if (![&] {
                for (const auto &instanceExtension : instanceExtensions)
                    if (std::string_view(instanceExtension.extensionName) == std::string_view(requiredExtension))
                        return true;
                return false;
            }())
                throw exception("Cannot find Vulkan instance extension: \"{}\"", requiredExtension);
        }

        return vk::raii::Instance(context, vk::InstanceCreateInfo{
            .pApplicationInfo = &applicationInfo,
            .enabledLayerCount = requiredLayers.size(),
            .ppEnabledLayerNames = requiredLayers.data(),
            .enabledExtensionCount = requiredInstanceExtensions.size(),
            .ppEnabledExtensionNames = requiredInstanceExtensions.data(),
        });
    }

    vk::raii::DebugReportCallbackEXT GPU::CreateDebugReportCallback(const DeviceState &state, const vk::raii::Instance &instance) {
        return vk::raii::DebugReportCallbackEXT(instance, vk::DebugReportCallbackCreateInfoEXT{
            .flags = vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::ePerformanceWarning | vk::DebugReportFlagBitsEXT::eInformation | vk::DebugReportFlagBitsEXT::eDebug,
            .pfnCallback = reinterpret_cast<PFN_vkDebugReportCallbackEXT>(&DebugCallback),
            .pUserData = state.logger.get(),
        });
    }

    GPU::GPU(const DeviceState &state) : presentation(state), instance(CreateInstance(state, context)), debugReportCallback(CreateDebugReportCallback(state, instance)) {}

    VkBool32 GPU::DebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char *layerPrefix, const char *message, Logger *logger) {
        constexpr std::array<Logger::LogLevel, 5> severityLookup{
            Logger::LogLevel::Info,  // VK_DEBUG_REPORT_INFORMATION_BIT_EXT
            Logger::LogLevel::Warn,  // VK_DEBUG_REPORT_WARNING_BIT_EXT
            Logger::LogLevel::Warn,  // VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
            Logger::LogLevel::Error, // VK_DEBUG_REPORT_ERROR_BIT_EXT
            Logger::LogLevel::Debug, // VK_DEBUG_REPORT_DEBUG_BIT_EXT
        };
        logger->Write(severityLookup.at(std::countr_zero(static_cast<u32>(flags))), util::Format("Vk{}:{}[0x{:X}]:I{}:L{}: {}", layerPrefix, vk::to_string(vk::DebugReportObjectTypeEXT(objectType)), object, messageCode, location, message));
        return VK_FALSE;
    }
}
