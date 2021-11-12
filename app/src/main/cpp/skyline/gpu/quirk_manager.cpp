// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "quirk_manager.h"

namespace skyline {
    QuirkManager::QuirkManager(vk::PhysicalDeviceProperties properties, vk::PhysicalDeviceFeatures2 features2, const std::vector<vk::ExtensionProperties> &extensions) {
        for (auto &extension : extensions) {
            #define EXT_SET(name, property) \
            case util::Hash(name):          \
                if (name == extensionName)  \
                    property = true;        \
                break

            #define EXT_SET_V(name, property, version)                    \
            case util::Hash(name):                                        \
                if (name == extensionName && extensionVersion >= version) \
                    property = true;                                      \
                break

            std::string_view extensionName{extension.extensionName};
            auto extensionVersion{extension.specVersion};
            switch (util::Hash(extensionName)) {
                EXT_SET("VK_EXT_provoking_vertex", supportsLastProvokingVertex);
            }

            #undef EXT_SET
            #undef EXT_SET_V
        }

        supportsLogicOp = features2.features.logicOp;
    }

    std::string QuirkManager::Summary() {
        return fmt::format("\n* Supports Last Provoking Vertex: {}\n* Supports Logical Operations: {}", supportsLastProvokingVertex, supportsLogicOp);
    }
}
