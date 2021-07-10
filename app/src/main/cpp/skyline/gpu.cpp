// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <jvm.h>
#include "gpu.h"

namespace skyline::gpu {
    vk::raii::Instance GPU::CreateInstance(const DeviceState &state, const vk::raii::Context &context) {
        vk::ApplicationInfo applicationInfo{
            .pApplicationName = "Skyline",
            .applicationVersion = state.jvm->GetVersionCode(), // Get the application version from JNI
            .pEngineName = "FTX1", // "Fast Tegra X1"
            .apiVersion = VkApiVersion,
        };

        #ifndef NDEBUG
        constexpr std::array<const char *, 0> requiredLayers{};
        #else
        constexpr std::array<const char *, 1> requiredLayers{
            "VK_LAYER_KHRONOS_validation"
        };
        #endif

        auto instanceLayers{context.enumerateInstanceLayerProperties()};
        if (state.logger->configLevel >= Logger::LogLevel::Debug) {
            std::string layers;
            for (const auto &instanceLayer : instanceLayers)
                layers += util::Format("\n* {} (Sv{}.{}.{}, Iv{}.{}.{}) - {}", instanceLayer.layerName, VK_VERSION_MAJOR(instanceLayer.specVersion), VK_VERSION_MINOR(instanceLayer.specVersion), VK_VERSION_PATCH(instanceLayer.specVersion), VK_VERSION_MAJOR(instanceLayer.implementationVersion), VK_VERSION_MINOR(instanceLayer.implementationVersion), VK_VERSION_PATCH(instanceLayer.implementationVersion), instanceLayer.description);
            state.logger->Debug("Vulkan Layers:{}", layers);
        }

        for (const auto &requiredLayer : requiredLayers) {
            if (!std::any_of(instanceLayers.begin(), instanceLayers.end(), [&](const vk::LayerProperties &instanceLayer) {
                return std::string_view(instanceLayer.layerName) == std::string_view(requiredLayer);
            }))
                throw exception("Cannot find Vulkan layer: \"{}\"", requiredLayer);
        }

        constexpr std::array<const char *, 3> requiredInstanceExtensions{
            VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
        };

        auto instanceExtensions{context.enumerateInstanceExtensionProperties()};
        if (state.logger->configLevel >= Logger::LogLevel::Debug) {
            std::string extensions;
            for (const auto &instanceExtension : instanceExtensions)
                extensions += util::Format("\n* {} (v{}.{}.{})", instanceExtension.extensionName, VK_VERSION_MAJOR(instanceExtension.specVersion), VK_VERSION_MINOR(instanceExtension.specVersion), VK_VERSION_PATCH(instanceExtension.specVersion));
            state.logger->Debug("Vulkan Instance Extensions:{}", extensions);
        }

        for (const auto &requiredExtension : requiredInstanceExtensions) {
            if (!std::any_of(instanceExtensions.begin(), instanceExtensions.end(), [&](const vk::ExtensionProperties &instanceExtension) {
                return std::string_view(instanceExtension.extensionName) == std::string_view(requiredExtension);
            }))
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

    vk::raii::PhysicalDevice GPU::CreatePhysicalDevice(const DeviceState &state, const vk::raii::Instance &instance) {
        return std::move(vk::raii::PhysicalDevices(instance).front()); // We just select the first device as we aren't expecting multiple GPUs
    }

    vk::raii::Device GPU::CreateDevice(const DeviceState &state, const vk::raii::PhysicalDevice &physicalDevice, typeof(vk::DeviceQueueCreateInfo::queueCount) &vkQueueFamilyIndex) {
        auto properties{physicalDevice.getProperties()}; // We should check for required properties here, if/when we have them

        // auto features{physicalDevice.getFeatures()}; // Same as above

        constexpr std::array<const char *, 1> requiredDeviceExtensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };
        auto deviceExtensions{physicalDevice.enumerateDeviceExtensionProperties()};
        for (const auto &requiredExtension : requiredDeviceExtensions) {
            if (!std::any_of(deviceExtensions.begin(), deviceExtensions.end(), [&](const vk::ExtensionProperties &deviceExtension) {
                return std::string_view(deviceExtension.extensionName) == std::string_view(requiredExtension);
            }))
                throw exception("Cannot find Vulkan device extension: \"{}\"", requiredExtension);
        }

        auto queueFamilies{physicalDevice.getQueueFamilyProperties()};
        float queuePriority{1.0f}; //!< The priority of the only queue we use, it's set to the maximum of 1.0
        vk::DeviceQueueCreateInfo queue{[&] {
            typeof(vk::DeviceQueueCreateInfo::queueFamilyIndex) index{};
            for (const auto &queueFamily : queueFamilies) {
                if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics && queueFamily.queueFlags & vk::QueueFlagBits::eCompute) {
                    vkQueueFamilyIndex = index;
                    return vk::DeviceQueueCreateInfo{
                        .queueFamilyIndex = index,
                        .queueCount = 1,
                        .pQueuePriorities = &queuePriority,
                    };
                }
                index++;
            }
            throw exception("Cannot find a queue family with both eGraphics and eCompute bits set");
        }()};

        if (state.logger->configLevel >= Logger::LogLevel::Info) {
            std::string extensionString;
            for (const auto &extension : deviceExtensions)
                extensionString += util::Format("\n* {} (v{}.{}.{})", extension.extensionName, VK_VERSION_MAJOR(extension.specVersion), VK_VERSION_MINOR(extension.specVersion), VK_VERSION_PATCH(extension.specVersion));

            std::string queueString;
            u32 familyIndex{};
            for (const auto &queueFamily : queueFamilies)
                queueString += util::Format("\n* {}x{}{}{}{}{}: TSB{} MIG({}x{}x{}){}", queueFamily.queueCount, queueFamily.queueFlags & vk::QueueFlagBits::eGraphics ? 'G' : '-', queueFamily.queueFlags & vk::QueueFlagBits::eCompute ? 'C' : '-', queueFamily.queueFlags & vk::QueueFlagBits::eTransfer ? 'T' : '-', queueFamily.queueFlags & vk::QueueFlagBits::eSparseBinding ? 'S' : '-', queueFamily.queueFlags & vk::QueueFlagBits::eProtected ? 'P' : '-', queueFamily.timestampValidBits, queueFamily.minImageTransferGranularity.width, queueFamily.minImageTransferGranularity.height, queueFamily.minImageTransferGranularity.depth, familyIndex++ == vkQueueFamilyIndex ? " <--" : "");

            state.logger->Info("Vulkan Device:\nName: {}\nType: {}\nVulkan Version: {}.{}.{}\nDriver Version: {}.{}.{}\nQueues:{}\nExtensions:{}", properties.deviceName, vk::to_string(properties.deviceType), VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion), VK_VERSION_PATCH(properties.apiVersion), VK_VERSION_MAJOR(properties.driverVersion), VK_VERSION_MINOR(properties.driverVersion), VK_VERSION_PATCH(properties.driverVersion), queueString, extensionString);
        }

        return vk::raii::Device(physicalDevice, vk::DeviceCreateInfo{
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queue,
            .enabledExtensionCount = requiredDeviceExtensions.size(),
            .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
        });
    }

    GPU::GPU(const DeviceState &state) : vkInstance(CreateInstance(state, vkContext)), vkDebugReportCallback(CreateDebugReportCallback(state, vkInstance)), vkPhysicalDevice(CreatePhysicalDevice(state, vkInstance)), vkDevice(CreateDevice(state, vkPhysicalDevice, vkQueueFamilyIndex)), vkQueue(vkDevice, vkQueueFamilyIndex, 0), memory(*this), scheduler(*this), presentation(state, *this) {}
}
