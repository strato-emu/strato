// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <adrenotools/driver.h>
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
        bool supportsAnisotropicFiltering{}; //!< If the device supports anisotropic filtering for sampling
        bool supportsLastProvokingVertex{}; //!< If the device supports setting the last vertex as the provoking vertex (with VK_EXT_provoking_vertex)
        bool supportsLogicOp{}; //!< If the device supports framebuffer logical operations during blending
        bool supportsVertexAttributeDivisor{}; //!< If the device supports a divisor for instance-rate vertex attributes (with VK_EXT_vertex_attribute_divisor)
        bool supportsVertexAttributeZeroDivisor{}; //!< If the device supports a zero divisor for instance-rate vertex attributes (with VK_EXT_vertex_attribute_divisor)
        bool supportsPushDescriptors{}; //!< If the device supports push descriptors (with VK_KHR_push_descriptor)
        bool supportsImageFormatList{}; //!< If the device supports providing a list of formats that can be used with an image (with VK_KHR_image_format_list)
        bool supportsImagelessFramebuffers{}; //!< If the device supports imageless framebuffers (with VK_KHR_imageless_framebuffer)
        bool supportsGlobalPriority{}; //!< If the device supports global priorities for queues (with VK_EXT_global_priority)
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
        bool supportsTransformFeedback{}; //!< If the 'VK_EXT_transform_feedback' extension is supported with neccessary features for emulation
        bool supportsImageReadWithoutFormat{}; //!< If a storage image can be read without a format
        bool supportsTopologyListRestart{}; //!< If the device supports using primitive restart for topology lists (with VK_EXT_primitive_topology_list_restart)
        bool supportsTopologyPatchListRestart{}; //!< If the device supports using primitive restart for topology patch lists (with VK_EXT_primitive_topology_list_restart)
        bool supportsGeometryShaders; //!< If the device supports the 'geometryShader' Vulkan feature
        bool supportsVertexPipelineStoresAndAtomics{}; //!< If the device supports the 'vertexPipelineStoresAndAtomics' Vulkan feature
        bool supportsFragmentStoresAndAtomics{}; //!< If the device supports the 'fragmentStoresAndAtomics' Vulkan feature
        bool supportsShaderStorageImageWriteWithoutFormat{}; //!< If the device supports the 'shaderStorageImageWriteWithoutFormat' Vulkan feature
        bool supportsSubgroupVote{}; //!< If subgroup votes are supported in shaders with SPV_KHR_subgroup_vote
        bool supportsWideLines{}; //!< If the device supports the 'wideLines' Vulkan feature
        bool supportsDepthClamp{}; //!< If the device supports the 'depthClamp' Vulkan feature
        bool supportsExtendedDynamicState{}; //!< If the device supports the 'VK_EXT_extended_dynamic_state' Vulkan extension
        bool supportsNullDescriptor{}; //!< If the device supports the null descriptor feature in the 'VK_EXT_robustness2' Vulkan extension
        u32 subgroupSize{}; //!< Size of a subgroup on the host GPU
        u32 hostVisibleCoherentCachedMemoryType{std::numeric_limits<u32>::max()};
        u32 minimumStorageBufferAlignment{}; //!< Minimum alignment for storage buffers passed to shaders

        u32 vendorId{}; //!< The `vendorID` Vulkan property
        u32 deviceId{}; //!< The `deviceID` Vulkan property
        u32 driverVersion{}; //!< The `driverVersion` Vulkan property
        std::array<u8, VK_UUID_SIZE> pipelineCacheUuid{}; //!< The `pipelineCacheUUID` Vulkan property

        std::bitset<7> bcnSupport{}; //!< Bitmask of BCn texture formats supported, it is ordered as BC1, BC2, BC3, BC4, BC5, BC6H and BC7
        bool supportsAdrenoDirectMemoryImport{};

        /**
         * @brief Manages a list of any vendor/device-specific errata in the host GPU
         */
        struct QuirkManager {
            bool needsIndividualTextureBindingWrites{}; //!< [Adreno Proprietary] A bug that requires descriptor set writes for VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER to be done individually with descriptorCount = 1 rather than batched
            bool vkImageMutableFormatCostly{}; //!< [Adreno Proprietary/Freedreno] An indication that VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT is costly and should not be enabled unless absolutely necessary (Disables UBWC on Adreno GPUs)
            bool adrenoRelaxedFormatAliasing{}; //!< [Adreno Proprietary/Freedreno] An indication that the GPU supports a relaxed version of view format aliasing without needing VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT, this is designed to work in tandem with 'vkImageMutableFormatCostly'
            bool adrenoBrokenFormatReport{}; //!< [Adreno Proprietary] If the drivers report format support incorrectly and include cases that are supported by the hardware
            bool relaxedRenderPassCompatibility{}; //!< [Adreno Proprietary/Freedreno] A relaxed version of Vulkan specification's render pass compatibility clause which allows for caching pipeline objects for multi-subpass renderpasses, this is intentionally disabled by default as it requires testing prior to enabling
            bool brokenPushDescriptors{}; //!< [Adreno Proprietary] A bug that causes push descriptor updates to ignored by the driver in certain situations
            bool brokenSpirvPositionInput{}; //!< [Adreno Proprietary] A bug that causes the shader compiler to fail on shaders with vertex position inputs not contained within a struct
            bool brokenSpirvAccessChainOpt{}; //!< [Adreno Proprietary] A broken optimisation pass causes dynamic access chain offsets to break
            bool brokenComputeShaders{}; //!< [ARM Proprietary] A bug that causes compute shaders in some games to crash the GPU
            bool brokenMultithreadedPipelineCompilation{}; //!< [Qualcomm Proprietary] A bug that causes the shader compiler to crash when compiling pipelines on multiple threads simultaneously
            bool brokenSubgroupMaskExtractDynamic{};  //!< [Qualcomm Proprietary] A bug that causes shaders using OpVectorExtractDynamic on the subgroup mask builtins to fail to compile
            bool brokenSubgroupShuffle{}; //!< [Qualcomm Proprietary] A bug that causes shaders using OpSubgroupShuffle to do all sorts of weird things
            bool brokenSpirvVectorAccessChain{}; //!< [Qualcomm Proprietary] A bug that causes SPIR-V OpAccessChains to work incorrectly when used to index vector arrays
            bool brokenDynamicStateVertexBindings{};  //!< [ARM Proprietary] A bug that causes VK_EXT_dynamic_state vertex bindings not to work correctly

            u32 maxSubpassCount{std::numeric_limits<u32>::max()}; //!< The maximum amount of subpasses within a renderpass, this is limited to 64 on older Adreno proprietary drivers
            vk::QueueGlobalPriorityEXT maxGlobalPriority{vk::QueueGlobalPriorityEXT::eMedium}; //!< The highest allowed global priority of the queue, drivers will not allow higher priorities to be set on queues

            QuirkManager() = default;

            QuirkManager(const vk::PhysicalDeviceProperties &deviceProperties, const vk::PhysicalDeviceDriverProperties &driverProperties);

            /**
             * @return A summary of all the GPU quirks as a human-readable string
             */
            std::string Summary();
        } quirks;

        TraitManager() = default;

        using DeviceProperties2 = vk::StructureChain<
            vk::PhysicalDeviceProperties2,
            vk::PhysicalDeviceDriverProperties,
            vk::PhysicalDeviceFloatControlsProperties,
            vk::PhysicalDeviceTransformFeedbackPropertiesEXT,
            vk::PhysicalDeviceSubgroupProperties>;

        using DeviceFeatures2 = vk::StructureChain<
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
            vk::PhysicalDeviceRobustness2FeaturesEXT>;

        TraitManager(const DeviceFeatures2 &deviceFeatures2, DeviceFeatures2 &enabledFeatures2, const std::vector<vk::ExtensionProperties> &deviceExtensions, std::vector<std::array<char, VK_MAX_EXTENSION_NAME_SIZE>> &enabledExtensions, const DeviceProperties2 &deviceProperties2, const vk::raii::PhysicalDevice &physicalDevice);

        /**
         * @brief Applies driver specific binary patches to the driver (e.g. BCeNabler)
         */
        void ApplyDriverPatches(const vk::raii::Context &context, adrenotools_gpu_mapping *mapping);

        /**
         * @return A summary of all the GPU traits as a human-readable string
         */
        std::string Summary();
    };
}
