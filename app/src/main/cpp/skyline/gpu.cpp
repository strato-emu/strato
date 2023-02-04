// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <dlfcn.h>
#include <adrenotools/driver.h>
#include <os.h>
#include <jvm.h>
#include <common/settings.h>
#include "gpu.h"

namespace skyline::gpu {
    static vk::raii::Instance CreateInstance(const DeviceState &state, const vk::raii::Context &context) {
        vk::ApplicationInfo applicationInfo{
            .pApplicationName = "Skyline",
            .applicationVersion = static_cast<uint32_t>(state.jvm->GetVersionCode()), // Get the application version from JNI
            .pEngineName = "FTX1", // "Fast Tegra X1"
            .apiVersion = VkApiVersion,
        };

        std::vector<const char *> requiredLayers{};
        #ifndef NDEBUG
        if (*state.settings->validationLayer)
            requiredLayers.push_back("VK_LAYER_KHRONOS_validation");
        #endif

        auto instanceLayers{context.enumerateInstanceLayerProperties()};
        if (Logger::configLevel >= Logger::LogLevel::Debug) {
            std::string layers;
            for (const auto &instanceLayer : instanceLayers)
                layers += util::Format("\n* {} (Sv{}.{}.{}, Iv{}.{}.{}) - {}", instanceLayer.layerName, VK_API_VERSION_MAJOR(instanceLayer.specVersion), VK_API_VERSION_MINOR(instanceLayer.specVersion), VK_API_VERSION_PATCH(instanceLayer.specVersion), VK_API_VERSION_MAJOR(instanceLayer.implementationVersion), VK_API_VERSION_MINOR(instanceLayer.implementationVersion), VK_API_VERSION_PATCH(instanceLayer.implementationVersion), instanceLayer.description);
            Logger::Debug("Vulkan Layers:{}", layers);
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
        if (Logger::configLevel >= Logger::LogLevel::Debug) {
            std::string extensions;
            for (const auto &instanceExtension : instanceExtensions)
                extensions += util::Format("\n* {} (v{}.{}.{})", instanceExtension.extensionName, VK_API_VERSION_MAJOR(instanceExtension.specVersion), VK_API_VERSION_MINOR(instanceExtension.specVersion), VK_API_VERSION_PATCH(instanceExtension.specVersion));
            Logger::Debug("Vulkan Instance Extensions:{}", extensions);
        }

        for (const auto &requiredExtension : requiredInstanceExtensions) {
            if (!std::any_of(instanceExtensions.begin(), instanceExtensions.end(), [&](const vk::ExtensionProperties &instanceExtension) {
                return std::string_view(instanceExtension.extensionName) == std::string_view(requiredExtension);
            }))
                throw exception("Cannot find Vulkan instance extension: \"{}\"", requiredExtension);
        }

        return vk::raii::Instance(context, vk::InstanceCreateInfo{
            .pApplicationInfo = &applicationInfo,
            .enabledLayerCount = static_cast<u32>(requiredLayers.size()),
            .ppEnabledLayerNames = requiredLayers.data(),
            .enabledExtensionCount = requiredInstanceExtensions.size(),
            .ppEnabledExtensionNames = requiredInstanceExtensions.data(),
        });
    }

    static VkBool32 DebugCallback(vk::DebugReportFlagsEXT flags, vk::DebugReportObjectTypeEXT objectType, u64 object, size_t location, i32 messageCode, const char *layerPrefix, const char *messageCStr, GPU *gpu) {
        constexpr std::array<Logger::LogLevel, 5> severityLookup{
            Logger::LogLevel::Info,  // VK_DEBUG_REPORT_INFORMATION_BIT_EXT
            Logger::LogLevel::Warn,  // VK_DEBUG_REPORT_WARNING_BIT_EXT
            Logger::LogLevel::Warn,  // VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
            Logger::LogLevel::Error, // VK_DEBUG_REPORT_ERROR_BIT_EXT
            Logger::LogLevel::Debug, // VK_DEBUG_REPORT_DEBUG_BIT_EXT
        };

        #define IGNORE_VALIDATION_C(string, function) \
        case util::Hash(string): {                    \
            if (string == type) {                     \
                function                              \
            }                                         \
            break;                                    \
        }

        #define IGNORE_VALIDATION_CL(string, functionName) IGNORE_VALIDATION_C(string, { if (!functionName()) return VK_FALSE; })

        #define IGNORE_VALIDATION(string) IGNORE_VALIDATION_C(string, { return VK_FALSE; })

        #define DEBUG_VALIDATION(string) IGNORE_VALIDATION_C(string, { raise(SIGTRAP); }) // Using __builtin_debugtrap() as opposed to raise(SIGTRAP) will result in the inability to continue

        std::string_view message{messageCStr};

        std::string_view type{message};
        auto first{type.find('[')};
        auto last{type.find(']', first)};
        if (first != std::string_view::npos && last != std::string_view::npos) {
            type = type.substr(first + 2, last != std::string_view::npos ? (last - first) - 3 : last);

            auto &traits{gpu->traits.quirks};

            auto returnIfBrokenFormat1{[&] {
                if (!traits.adrenoRelaxedFormatAliasing)
                    return true;

                constexpr std::string_view FormatTag{"VK_FORMAT_"};
                auto start{message.find(FormatTag)}, end{message.find(' ', start)};
                if (start == std::string_view::npos || end == std::string_view::npos)
                    return true;

                std::string_view formatName{message.data() + start + FormatTag.length(), message.data() + end};
                if (formatName.ends_with(')'))
                    formatName.remove_suffix(1);

                if (formatName.starts_with("BC") && formatName.ends_with("_BLOCK"))
                    return false; // BCn formats

                #define FMT(name) if (formatName == name) return false

                FMT("B5G6R5_UNORM_PACK16");
                FMT("R5G6B5_UNORM_PACK16");
                FMT("R32G32B32A32_SFLOAT");
                FMT("D32_SFLOAT");
                FMT("R32_SFLOAT");

                #undef FMT

                return true;
            }};

            auto returnIfBrokenFormat2{[&] {
                if (!traits.adrenoRelaxedFormatAliasing)
                    return true;

                constexpr std::string_view FormatTag{"format"}; // The format is provided as "format {}" where {} is the VkFormat value in numerical form
                auto formatNumber{message.find_first_of("0123456789", message.find(FormatTag) + FormatTag.size())};
                if (formatNumber != std::string_view::npos) {
                    auto format{static_cast<vk::Format>(std::stoi(std::string{message.substr(formatNumber)}))};
                    switch (format) {
                        case vk::Format::eR5G6B5UnormPack16:
                        case vk::Format::eB5G6R5UnormPack16:
                        case vk::Format::eR32G32B32A32Sfloat:
                        case vk::Format::eD32Sfloat:

                        case vk::Format::eBc1RgbUnormBlock:
                        case vk::Format::eBc1RgbSrgbBlock:
                        case vk::Format::eBc1RgbaUnormBlock:
                        case vk::Format::eBc1RgbaSrgbBlock:
                        case vk::Format::eBc2UnormBlock:
                        case vk::Format::eBc2SrgbBlock:
                        case vk::Format::eBc3UnormBlock:
                        case vk::Format::eBc3SrgbBlock:
                        case vk::Format::eBc4UnormBlock:
                        case vk::Format::eBc4SnormBlock:
                        case vk::Format::eBc5UnormBlock:
                        case vk::Format::eBc5SnormBlock:
                        case vk::Format::eBc6HUfloatBlock:
                        case vk::Format::eBc6HSfloatBlock:
                        case vk::Format::eBc7UnormBlock:
                        case vk::Format::eBc7SrgbBlock:
                            return false;

                        default:
                            return true;
                    }
                }

                return true;
            }};

            switch (util::Hash(type)) {
                IGNORE_VALIDATION("UNASSIGNED-CoreValidation-SwapchainPreTransform") // We handle transformation via Android APIs directly
                IGNORE_VALIDATION("UNASSIGNED-GeneralParameterPerfWarn-SuboptimalSwapchain") // Same as SwapchainPreTransform
                IGNORE_VALIDATION("UNASSIGNED-CoreValidation-DrawState-InvalidImageLayout") // We utilize images as VK_IMAGE_LAYOUT_GENERAL rather than optimal layouts for operations
                IGNORE_VALIDATION("VUID-VkImageViewCreateInfo-image-01762") // We allow aliasing of certain formats and handle warning in other cases ourselves

                /* BCn format missing due to adrenotools */
                IGNORE_VALIDATION_CL("VUID-VkImageCreateInfo-imageCreateMaxMipLevels-02251", returnIfBrokenFormat1)
                IGNORE_VALIDATION_CL("VUID-VkImageViewCreateInfo-None-02273", returnIfBrokenFormat1)
                IGNORE_VALIDATION_CL("VUID-VkImageViewCreateInfo-usage-02274", returnIfBrokenFormat1)
                IGNORE_VALIDATION_CL("VUID-vkCmdDraw-magFilter-04553", returnIfBrokenFormat1)
                IGNORE_VALIDATION_CL("VUID-vkCmdDrawIndexed-magFilter-04553", returnIfBrokenFormat1)
                IGNORE_VALIDATION_CL("VUID-vkCmdCopyImageToBuffer-srcImage-01998", returnIfBrokenFormat2)
                IGNORE_VALIDATION_CL("VUID-vkCmdCopyBufferToImage-dstImage-01997", returnIfBrokenFormat2)

                /* Guest driven performance warnings, these cannot be fixed by us */
                IGNORE_VALIDATION("UNASSIGNED-CoreValidation-Shader-InputNotProduced")
                IGNORE_VALIDATION("UNASSIGNED-CoreValidation-Shader-OutputNotConsumed")

                /* Pipeline Cache isn't compliant with the Vulkan specification, it depends on driver support for a relaxed version of Vulkan specification's Render Pass Compatibility clause and this will result in validation errors regardless which we need to ignore */
                IGNORE_VALIDATION("VUID-vkCmdDraw-renderPass-02684")
                IGNORE_VALIDATION("VUID-vkCmdDraw-subpass-02685")
                IGNORE_VALIDATION("VUID-vkCmdDrawIndexed-renderPass-02684")
                IGNORE_VALIDATION("VUID-vkCmdDrawIndexed-subpass-02685")
            }

            #undef IGNORE_TYPE
        }

        Logger::Write(severityLookup.at(static_cast<size_t>(std::countr_zero(static_cast<u32>(flags)))), util::Format("Vk{}:{}[0x{:X}]:I{}:L{}: {}", layerPrefix, vk::to_string(vk::DebugReportObjectTypeEXT(objectType)), object, messageCode, location, message));

        return VK_FALSE;
    }

    static vk::raii::DebugReportCallbackEXT CreateDebugReportCallback(GPU *gpu, const vk::raii::Instance &instance) {
        return vk::raii::DebugReportCallbackEXT(instance, vk::DebugReportCallbackCreateInfoEXT{
            .flags = vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::ePerformanceWarning | vk::DebugReportFlagBitsEXT::eInformation | vk::DebugReportFlagBitsEXT::eDebug,
            .pfnCallback = reinterpret_cast<PFN_vkDebugReportCallbackEXT>(&DebugCallback),
            .pUserData = gpu,
        });
    }

    static vk::raii::PhysicalDevice CreatePhysicalDevice(const vk::raii::Instance &instance) {
        auto devices{vk::raii::PhysicalDevices(instance)};
        if (devices.empty())
            throw exception("No Vulkan physical devices found");
        return std::move(devices.front()); // We just select the first device as we aren't expecting multiple GPUs
    }

    static vk::raii::Device CreateDevice(const vk::raii::Context &context,
                                         const vk::raii::PhysicalDevice &physicalDevice,
                                         decltype(vk::DeviceQueueCreateInfo::queueCount) &vkQueueFamilyIndex,
                                         TraitManager &traits,
                                         adrenotools_gpu_mapping *mapping) {
        auto deviceFeatures2{physicalDevice.getFeatures2<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceCustomBorderColorFeaturesEXT,
            vk::PhysicalDeviceVertexAttributeDivisorFeaturesEXT,
            vk::PhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT,
            vk::PhysicalDeviceShaderFloat16Int8Features,
            vk::PhysicalDeviceShaderAtomicInt64Features,
            vk::PhysicalDeviceUniformBufferStandardLayoutFeatures,
            vk::PhysicalDeviceShaderDrawParametersFeatures,
            vk::PhysicalDeviceProvokingVertexFeaturesEXT,
            vk::PhysicalDevicePrimitiveTopologyListRestartFeaturesEXT,
            vk::PhysicalDeviceImagelessFramebufferFeatures,
            vk::PhysicalDeviceTransformFeedbackFeaturesEXT,
            vk::PhysicalDeviceIndexTypeUint8FeaturesEXT,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
            vk::PhysicalDeviceRobustness2FeaturesEXT>()};
        decltype(deviceFeatures2) enabledFeatures2{}; // We only want to enable features we required due to potential overhead from unused features

        #define FEAT_REQ(structName, feature)                                            \
        if (deviceFeatures2.get<structName>().feature)                                   \
            enabledFeatures2.get<structName>().feature = true;                           \
        else                                                                             \
            throw exception("Vulkan device doesn't support required feature: " #feature)

        FEAT_REQ(vk::PhysicalDeviceFeatures2, features.independentBlend);
        FEAT_REQ(vk::PhysicalDeviceFeatures2, features.shaderImageGatherExtended);
        FEAT_REQ(vk::PhysicalDeviceFeatures2, features.depthBiasClamp);
        FEAT_REQ(vk::PhysicalDeviceShaderDrawParametersFeatures, shaderDrawParameters);

        #undef FEAT_REQ

        auto deviceExtensions{physicalDevice.enumerateDeviceExtensionProperties()};
        std::vector<std::array<char, VK_MAX_EXTENSION_NAME_SIZE>> enabledExtensions{
            {
                // Required Extensions
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            }
        };

        for (const auto &requiredExtension : enabledExtensions) {
            if (!std::any_of(deviceExtensions.begin(), deviceExtensions.end(), [&](const vk::ExtensionProperties &deviceExtension) {
                return std::string_view(deviceExtension.extensionName) == std::string_view(requiredExtension.data());
            }))
                throw exception("Cannot find Vulkan device extension: \"{}\"", requiredExtension.data());
        }

        auto deviceProperties2{physicalDevice.getProperties2<
            vk::PhysicalDeviceProperties2,
            vk::PhysicalDeviceDriverProperties,
            vk::PhysicalDeviceFloatControlsProperties,
            vk::PhysicalDeviceTransformFeedbackPropertiesEXT,
            vk::PhysicalDeviceSubgroupProperties>()};

        traits = TraitManager{deviceFeatures2, enabledFeatures2, deviceExtensions, enabledExtensions, deviceProperties2, physicalDevice};
        traits.ApplyDriverPatches(context, mapping);

        std::vector<const char *> pEnabledExtensions;
        pEnabledExtensions.reserve(enabledExtensions.size());
        for (auto &extension : enabledExtensions)
            pEnabledExtensions.push_back(extension.data());

        auto queueFamilies{physicalDevice.getQueueFamilyProperties()};
        float queuePriority{1.0f}; //!< The priority of the only queue we use, it's set to the maximum of 1.0
        vk::StructureChain<vk::DeviceQueueCreateInfo, vk::DeviceQueueGlobalPriorityCreateInfoEXT> queueCreateInfo{
            [&]() -> vk::DeviceQueueCreateInfo {
                decltype(vk::DeviceQueueCreateInfo::queueFamilyIndex) index{};
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
            }(),
            vk::DeviceQueueGlobalPriorityCreateInfoEXT{
                .globalPriority = traits.quirks.maxGlobalPriority,
            }
        };

        if (!traits.supportsGlobalPriority)
            queueCreateInfo.unlink<vk::DeviceQueueGlobalPriorityCreateInfoEXT>();

        if (Logger::configLevel >= Logger::LogLevel::Info) {
            std::string extensionString;
            for (const auto &extension : deviceExtensions)
                extensionString += util::Format("\n* {} (v{}.{}.{})", extension.extensionName, VK_API_VERSION_MAJOR(extension.specVersion), VK_API_VERSION_MINOR(extension.specVersion), VK_API_VERSION_PATCH(extension.specVersion));

            std::string queueString;
            u32 familyIndex{};
            for (const auto &queueFamily : queueFamilies)
                queueString += util::Format("\n* {}x{}{}{}{}{}: TSB{} MIG({}x{}x{}){}", queueFamily.queueCount, queueFamily.queueFlags & vk::QueueFlagBits::eGraphics ? 'G' : '-', queueFamily.queueFlags & vk::QueueFlagBits::eCompute ? 'C' : '-', queueFamily.queueFlags & vk::QueueFlagBits::eTransfer ? 'T' : '-', queueFamily.queueFlags & vk::QueueFlagBits::eSparseBinding ? 'S' : '-', queueFamily.queueFlags & vk::QueueFlagBits::eProtected ? 'P' : '-', queueFamily.timestampValidBits, queueFamily.minImageTransferGranularity.width, queueFamily.minImageTransferGranularity.height, queueFamily.minImageTransferGranularity.depth, familyIndex++ == vkQueueFamilyIndex ? " <--" : "");

            auto properties{deviceProperties2.get<vk::PhysicalDeviceProperties2>().properties};
            Logger::Info("Vulkan Device:\nName: {}\nType: {}\nDriver ID: {}\nVulkan Version: {}.{}.{}\nDriver Version: {}.{}.{}\nQueues:{}\nExtensions:{}\nTraits:{}\nQuirks:{}",
                         properties.deviceName, vk::to_string(properties.deviceType),
                         vk::to_string(deviceProperties2.get<vk::PhysicalDeviceDriverProperties>().driverID),
                         VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion),
                         VK_API_VERSION_MAJOR(properties.driverVersion), VK_API_VERSION_MINOR(properties.driverVersion), VK_API_VERSION_PATCH(properties.driverVersion),
                         queueString, extensionString, traits.Summary(), traits.quirks.Summary());
        }

        return vk::raii::Device(physicalDevice, vk::DeviceCreateInfo{
            .pNext = &enabledFeatures2,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo.get<vk::DeviceQueueCreateInfo>(),
            .enabledExtensionCount = static_cast<uint32_t>(pEnabledExtensions.size()),
            .ppEnabledExtensionNames = pEnabledExtensions.data(),
        });
    }

    static PFN_vkGetInstanceProcAddr LoadVulkanDriver(const DeviceState &state, adrenotools_gpu_mapping *mapping) {
        void *libvulkanHandle{};

        // If the user has selected a custom driver, try to load it
        if (!(*state.settings->gpuDriver).empty()) {
            libvulkanHandle = adrenotools_open_libvulkan(
                RTLD_NOW,
                ADRENOTOOLS_DRIVER_FILE_REDIRECT | ADRENOTOOLS_DRIVER_CUSTOM | ADRENOTOOLS_DRIVER_GPU_MAPPING_IMPORT,
                nullptr, // We require Android 10 so don't need to supply
                state.os->nativeLibraryPath.c_str(),
                (state.os->privateAppFilesPath + "gpu_drivers/" + *state.settings->gpuDriver + "/").c_str(),
                (*state.settings->gpuDriverLibraryName).c_str(),
                (state.os->publicAppFilesPath + "gpu/vk_file_redirect/").c_str(),
                mapping
            );

            if (!libvulkanHandle) {
                char *error = dlerror();
                Logger::Warn("Failed to load custom Vulkan driver {}/{}: {}", *state.settings->gpuDriver, *state.settings->gpuDriverLibraryName, error ? error : "");
            }
        }

        if (!libvulkanHandle) {
            libvulkanHandle = adrenotools_open_libvulkan(
                RTLD_NOW,
                ADRENOTOOLS_DRIVER_FILE_REDIRECT | ADRENOTOOLS_DRIVER_GPU_MAPPING_IMPORT,
                nullptr, // We require Android 10 so don't need to supply
                state.os->nativeLibraryPath.c_str(),
                nullptr,
                nullptr,
                (state.os->publicAppFilesPath + "gpu/vk_file_redirect/").c_str(),
                mapping
            );

            if (!libvulkanHandle) {
                char *error = dlerror();
                Logger::Warn("Failed to load builtin Vulkan driver: {}", error ? error : "");
            }

            if (!libvulkanHandle)
                libvulkanHandle = dlopen("libvulkan.so", RTLD_NOW);
        }

        return reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(libvulkanHandle, "vkGetInstanceProcAddr"));
    }

    GPU::GPU(const DeviceState &state)
        : state(state),
          vkContext(LoadVulkanDriver(state, &adrenotoolsImportMapping)),
          vkInstance(CreateInstance(state, vkContext)),
          vkDebugReportCallback(CreateDebugReportCallback(this, vkInstance)),
          vkPhysicalDevice(CreatePhysicalDevice(vkInstance)),
          vkDevice(CreateDevice(vkContext, vkPhysicalDevice, vkQueueFamilyIndex, traits, &adrenotoolsImportMapping)),
          vkQueue(vkDevice, vkQueueFamilyIndex, 0),
          memory(*this),
          scheduler(state, *this),
          presentation(state, *this),
          texture(*this),
          buffer(*this),
          megaBufferAllocator(*this),
          descriptor(*this),
          helperShaders(*this, state.os->assetFileSystem),
          renderPassCache(*this),
          framebufferCache(*this),
          debugTracingBuffer(memory.AllocateBuffer(DebugTracingBufferSize)) {}

    void GPU::Initialise() {
        std::string titleId{state.loader->nacp->GetSaveDataOwnerId()};
        graphicsPipelineAssembler.emplace(*this, state.os->publicAppFilesPath + "vk_graphics_pipeline_cache/" + titleId);
        shader.emplace(state, *this,
                       state.os->publicAppFilesPath + "shader_replacements/" + titleId,
                       state.os->publicAppFilesPath + "shader_dumps/" + titleId);
        if (!*state.settings->disableShaderCache)
            graphicsPipelineCacheManager.emplace(state,
                                                 state.os->publicAppFilesPath + "graphics_pipeline_cache/" + titleId);
        graphicsPipelineManager.emplace(*this);
    }
}
