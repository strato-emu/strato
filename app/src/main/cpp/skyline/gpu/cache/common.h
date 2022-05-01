// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <common.h>

namespace skyline::gpu::cache {
    /**
     * @brief All unique metadata in a single attachment for a compatible render pass according to Render Pass Compatibility clause in the Vulkan specification
     * @url https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#renderpass-compatibility
     * @url https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkAttachmentDescription.html
     */
    struct AttachmentMetadata {
        vk::Format format;
        vk::SampleCountFlagBits sampleCount;

        constexpr AttachmentMetadata(vk::Format format, vk::SampleCountFlagBits sampleCount) : format(format), sampleCount(sampleCount) {}

        bool operator==(const AttachmentMetadata &rhs) const = default;
    };
}
