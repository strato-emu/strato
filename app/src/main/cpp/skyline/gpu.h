// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "gpu/trait_manager.h"
#include "gpu/memory_manager.h"
#include "gpu/command_scheduler.h"
#include "gpu/presentation_engine.h"
#include "gpu/texture_manager.h"
#include "gpu/buffer_manager.h"
#include "gpu/megabuffer.h"
#include "gpu/descriptor_allocator.h"
#include "gpu/shader_manager.h"
#include "gpu/shaders/helper_shaders.h"
#include "gpu/cache/graphics_pipeline_cache.h"
#include "gpu/cache/renderpass_cache.h"
#include "gpu/cache/framebuffer_cache.h"

namespace skyline::gpu {
    static constexpr u32 VkApiVersion{VK_API_VERSION_1_1}; //!< The version of core Vulkan that we require

    /**
     * @brief An interface to host GPU structures, anything concerning host GPU/Presentation APIs is encapsulated by this
     */
    class GPU {
      private:
        const DeviceState &state; // We access the device state inside Texture (and Buffers) for setting up NCE memory tracking
        friend Texture;
        friend Buffer;

      public:
        vk::raii::Context vkContext;
        vk::raii::Instance vkInstance;
        vk::raii::DebugReportCallbackEXT vkDebugReportCallback; //!< An RAII Vulkan debug report manager which calls into 'GPU::DebugCallback'
        vk::raii::PhysicalDevice vkPhysicalDevice;
        u32 vkQueueFamilyIndex{};
        TraitManager traits;
        vk::raii::Device vkDevice;
        std::mutex queueMutex; //!< Synchronizes access to the queue as it is externally synchronized
        vk::raii::Queue vkQueue; //!< A Vulkan Queue supporting graphics and compute operations

        memory::MemoryManager memory;
        CommandScheduler scheduler;
        PresentationEngine presentation;

        TextureManager texture;
        BufferManager buffer;
        MegaBufferAllocator megaBufferAllocator;

        DescriptorAllocator descriptor;
        ShaderManager shader;

        HelperShaders helperShaders;

        cache::GraphicsPipelineCache graphicsPipelineCache;
        cache::RenderPassCache renderPassCache;
        cache::FramebufferCache framebufferCache;

        GPU(const DeviceState &state);
    };
}
