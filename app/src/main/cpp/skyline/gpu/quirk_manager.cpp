// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "quirk_manager.h"

namespace skyline {
    QuirkManager::QuirkManager(const vk::PhysicalDeviceProperties &properties, vk::PhysicalDeviceFeatures2 &features2, const std::vector<vk::ExtensionProperties> &deviceExtensions, std::vector<std::array<char, VK_MAX_EXTENSION_NAME_SIZE>> &enabledExtensions) {
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
                if (name == extensionName && extensionVersion >= version)                            \
                    property = true;                                                                 \
                    enabledExtensions.push_back(std::array<char, VK_MAX_EXTENSION_NAME_SIZE>{name}); \
                break

            std::string_view extensionName{extension.extensionName};
            auto extensionVersion{extension.specVersion};
            switch (util::Hash(extensionName)) {
                EXT_SET("VK_EXT_provoking_vertex", supportsLastProvokingVertex);
            }

            #undef EXT_SET
            #undef EXT_SET_V
        }

        auto deviceFeatures2{features2};
        features2 = vk::PhysicalDeviceFeatures2{}; // We only want to enable features we required due to potential overhead from unused features

        #define FEAT_SET(feature, property)     \
        if (deviceFeatures2.features.feature) { \
            property = true;                    \
            features2.features.feature = true;  \
        }

        FEAT_SET(logicOp, supportsLogicOp)

        #undef FEAT_SET
    }

    std::string QuirkManager::Summary() {
        return fmt::format("\n* Supports Last Provoking Vertex: {}\n* Supports Logical Operations: {}", supportsLastProvokingVertex, supportsLogicOp);
    }
}
