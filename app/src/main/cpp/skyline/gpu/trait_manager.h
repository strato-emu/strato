// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <common.h>

namespace skyline::gpu {
    /**
     * @brief Checks and stores all the traits of the host GPU discovered at runtime
     */
    class TraitManager {
      public:
        bool supportsUint8Indices{}; //!< If the device supports using uint8 indices in index buffers (with VK_EXT_index_type_uint8)
        bool supportsSamplerMirrorClampToEdge{}; //!< If the device supports a mirrored clamp to edge as a sampler address mode (with VK_KHR_sampler_mirror_clamp_to_edge)
        bool supportsSamplerReductionMode{}; //!< If the device supports explicitly specifying a reduction mode for sampling (with VK_EXT_sampler_filter_minmax)
        bool supportsCustomBorderColor{}; //!< If the device supports a custom border color without format (VK_EXT_custom_border_color)
        bool supportsLastProvokingVertex{}; //!< If the device supports setting the last vertex as the provoking vertex (with VK_EXT_provoking_vertex)
        bool supportsLogicOp{}; //!< If the device supports framebuffer logical operations during blending
        bool supportsVertexAttributeDivisor{}; //!< If the device supports a divisor for instance-rate vertex attributes (with VK_EXT_vertex_attribute_divisor)
        bool supportsVertexAttributeZeroDivisor{}; //!< If the device supports a zero divisor for instance-rate vertex attributes (with VK_EXT_vertex_attribute_divisor)
        bool supportsMultipleViewports{}; //!< If the device supports more than one viewport
        bool supportsShaderViewportIndexLayer{}; //!< If the device supports retrieving the viewport index in shaders (with VK_EXT_shader_viewport_index_layer)
        bool supportsSpirv14{}; //!< If SPIR-V 1.4 is supported (with VK_KHR_spirv_1_4)
        bool supportsShaderDemoteToHelper{}; //!< If a shader invocation can be demoted to a helper invocation (with VK_EXT_shader_demote_to_helper_invocation)
        bool supportsFloat16{}; //!< If 16-bit floating point integers are supported in shaders
        bool supportsInt8{}; //!< If 8-bit integers are supported in shaders
        bool supportsInt16{}; //!< If 16-bit integers are supported in shaders
        bool supportsInt64{}; //!< If 64-bit integers are supported in shaders
        bool supportsAtomicInt64{}; //!< If atomic operations on 64-bit integers are supported in shaders
        bool supportsFloatControls{}; //!< If extensive control over FP behavior is exposed (with VK_KHR_shader_float_controls)
        vk::PhysicalDeviceFloatControlsProperties floatControls{}; //!< Specifics of FP behavior control (All members will be zero'd out when unavailable)
        bool supportsImageReadWithoutFormat{}; //!< If a storage image can be read without a format
        bool supportsSubgroupVote{}; //!< If subgroup votes are supported in shaders with SPV_KHR_subgroup_vote
        u32 subgroupSize{}; //!< Size of a subgroup on the host GPU

        /**
         * @brief Manages a list of any vendor/device-specific errata in the host GPU
         */
        struct QuirkManager {
            bool needsIndividualTextureBindingWrites{}; //!< [Adreno Proprietary] A bug that requires descriptor set writes for VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER to be done individually with descriptorCount = 1 rather than batched
            bool vkImageMutableFormatCostly{}; //!< [Adreno Proprietary/Freedreno] An indication that VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT is costly and should not be enabled unless absolutely necessary (Disables UBWC on Adreno GPUs)

            QuirkManager() = default;

            QuirkManager(const vk::PhysicalDeviceProperties& deviceProperties, const vk::PhysicalDeviceDriverProperties& driverProperties);

            /**
             * @return A summary of all the GPU quirks as a human-readable string
             */
            std::string Summary();
        } quirks;

        TraitManager() = default;

        using DeviceProperties2 = vk::StructureChain<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDriverProperties, vk::PhysicalDeviceFloatControlsProperties, vk::PhysicalDeviceSubgroupProperties>;

        using DeviceFeatures2 = vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceCustomBorderColorFeaturesEXT, vk::PhysicalDeviceVertexAttributeDivisorFeaturesEXT, vk::PhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT, vk::PhysicalDeviceShaderFloat16Int8Features, vk::PhysicalDeviceShaderAtomicInt64Features, vk::PhysicalDeviceUniformBufferStandardLayoutFeatures, vk::PhysicalDeviceShaderDrawParametersFeatures, vk::PhysicalDeviceProvokingVertexFeaturesEXT>;

        TraitManager(const DeviceFeatures2 &deviceFeatures2, DeviceFeatures2 &enabledFeatures2, const std::vector<vk::ExtensionProperties> &deviceExtensions, std::vector<std::array<char, VK_MAX_EXTENSION_NAME_SIZE>> &enabledExtensions, const DeviceProperties2 &deviceProperties2);

        /**
         * @brief Applies driver specific binary patches to the driver (e.g. BCeNabler)
         */
        void ApplyDriverPatches(const vk::raii::Context &context);

        /**
         * @return A summary of all the GPU traits as a human-readable string
         */
        std::string Summary();
    };
}
