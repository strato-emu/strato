// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "quirk_manager.h"

namespace skyline::gpu {
    QuirkManager::QuirkManager(const DeviceFeatures2 &deviceFeatures2, DeviceFeatures2 &enabledFeatures2, const std::vector<vk::ExtensionProperties> &deviceExtensions, std::vector<std::array<char, VK_MAX_EXTENSION_NAME_SIZE>> &enabledExtensions, const DeviceProperties2 &deviceProperties2) {
        bool hasShaderAtomicInt64{}, hasShaderFloat16Int8Ext{};

        for (auto &extension : deviceExtensions) {
            #define EXT_SET(name, property)                                                          \
            case util::Hash(name):                                                                   \
                if (name == extensionName) {                                                         \
                    property = true;                                                                 \
                    enabledExtensions.push_back(std::array<char, VK_MAX_EXTENSION_NAME_SIZE>{name}); \
                }                                                                                    \
                break

            #define EXT_SET_V(name, property, version)                                               \
            case util::Hash(name):                                                                   \
                if (name == extensionName && extensionVersion >= version) {                          \
                    property = true;                                                                 \
                    enabledExtensions.push_back(std::array<char, VK_MAX_EXTENSION_NAME_SIZE>{name}); \
                }                                                                                    \
                break

            std::string_view extensionName{extension.extensionName};
            auto extensionVersion{extension.specVersion};
            switch (util::Hash(extensionName)) {
                EXT_SET("VK_EXT_provoking_vertex", supportsLastProvokingVertex);
                EXT_SET("VK_EXT_vertex_attribute_divisor", supportsVertexAttributeDivisor);
                EXT_SET("VK_KHR_spirv_1_4", supportsSpirv14);
                EXT_SET("VK_KHR_shader_atomic_int64", hasShaderAtomicInt64);
                EXT_SET("VK_KHR_shader_float16_int8", hasShaderFloat16Int8Ext);
                EXT_SET("VK_KHR_shader_float_controls", supportsFloatControls);
            }

            #undef EXT_SET
            #undef EXT_SET_V
        }

        #define FEAT_SET(structName, feature, property)        \
        if (deviceFeatures2.get<structName>().feature) {       \
            property = true;                                   \
            enabledFeatures2.get<structName>().feature = true; \
        }

        FEAT_SET(vk::PhysicalDeviceFeatures2, features.logicOp, supportsLogicOp)
        FEAT_SET(vk::PhysicalDeviceFeatures2, features.multiViewport, supportsMultipleViewports)
        FEAT_SET(vk::PhysicalDeviceFeatures2, features.shaderInt16, supportsInt16)
        FEAT_SET(vk::PhysicalDeviceFeatures2, features.shaderInt64, supportsInt64)
        FEAT_SET(vk::PhysicalDeviceFeatures2, features.shaderStorageImageReadWithoutFormat, supportsImageReadWithoutFormat)

        if (supportsVertexAttributeDivisor) {
            FEAT_SET(vk::PhysicalDeviceVertexAttributeDivisorFeaturesEXT, vertexAttributeInstanceRateDivisor, supportsVertexAttributeDivisor)
            FEAT_SET(vk::PhysicalDeviceVertexAttributeDivisorFeaturesEXT, vertexAttributeInstanceRateZeroDivisor, supportsVertexAttributeZeroDivisor)
        } else {
            enabledFeatures2.unlink<vk::PhysicalDeviceVertexAttributeDivisorFeaturesEXT>();
        }

        auto &shaderAtomicFeatures{deviceFeatures2.get<vk::PhysicalDeviceShaderAtomicInt64Features>()};
        if (hasShaderAtomicInt64 && shaderAtomicFeatures.shaderBufferInt64Atomics && shaderAtomicFeatures.shaderSharedInt64Atomics) {
            supportsAtomicInt64 = true;
        } else {
            enabledFeatures2.unlink<vk::PhysicalDeviceShaderAtomicInt64Features>();
        }

        if (hasShaderFloat16Int8Ext) {
            FEAT_SET(vk::PhysicalDeviceShaderFloat16Int8Features, shaderFloat16, supportsFloat16)
            FEAT_SET(vk::PhysicalDeviceShaderFloat16Int8Features, shaderInt8, supportsInt8)
        } else {
            enabledFeatures2.unlink<vk::PhysicalDeviceShaderFloat16Int8Features>();
        }

        #undef FEAT_SET

        if (supportsFloatControls)
            floatControls = deviceProperties2.get<vk::PhysicalDeviceFloatControlsProperties>();

        auto &subgroupProperties{deviceProperties2.get<vk::PhysicalDeviceSubgroupProperties>()};
        supportsSubgroupVote = static_cast<bool>(subgroupProperties.supportedOperations & vk::SubgroupFeatureFlagBits::eVote);
        subgroupSize = deviceProperties2.get<vk::PhysicalDeviceSubgroupProperties>().subgroupSize;
    }

    std::string QuirkManager::Summary() {
        return fmt::format("\n* Supports Last Provoking Vertex: {}\n* Supports Logical Operations: {}\n* Supports Vertex Attribute Divisor: {}\n* Supports Vertex Attribute Zero Divisor: {}\n* Supports Multiple Viewports: {}\n* Supports SPIR-V 1.4: {}\n* Supports 16-bit FP: {}\n* Supports 8-bit Integers: {}\n* Supports 16-bit Integers: {}\n* Supports 64-bit Integers: {}\n* Supports Atomic 64-bit Integers: {}\n* Supports Floating Point Behavior Control: {}\n* Supports Image Read Without Format: {}\n* Supports Subgroup Vote: {}\n* Subgroup Size: {}", supportsLastProvokingVertex, supportsLogicOp, supportsVertexAttributeDivisor, supportsVertexAttributeZeroDivisor, supportsMultipleViewports, supportsSpirv14, supportsFloat16, supportsInt8, supportsInt16, supportsInt64, supportsAtomicInt64, supportsFloatControls, supportsImageReadWithoutFormat, supportsSubgroupVote, subgroupSize);
    }
}
