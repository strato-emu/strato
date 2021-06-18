// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <android/looper.h>
#include <common/trace.h>
#include <kernel/types/KEvent.h>
#include <services/hosbinder/GraphicBufferProducer.h>
#include "texture/texture.h"

struct ANativeWindow;

namespace skyline::gpu {
    /**
     * @brief All host presentation is handled by this, it manages the host surface and swapchain alongside dynamically recreating it when required
     */
    class PresentationEngine {
      private:
        const DeviceState &state;
        GPU &gpu;

        std::mutex mutex; //!< Synchronizes access to the surface objects
        std::condition_variable surfaceCondition; //!< Allows us to efficiently wait for Vulkan surface to be initialized
        jobject jSurface{}; //!< The Java Surface object backing the ANativeWindow

        std::optional<vk::raii::SurfaceKHR> vkSurface; //!< The Vulkan Surface object that is backed by ANativeWindow
        vk::SurfaceCapabilitiesKHR vkSurfaceCapabilities; //!< The capabilities of the current Vulkan Surface

        std::optional<vk::raii::SwapchainKHR> vkSwapchain; //!< The Vulkan swapchain and the properties associated with it
        vk::raii::Fence acquireFence; //!< A fence for acquiring an image from the swapchain
        texture::Format swapchainFormat{}; //!< The image format of the textures in the current swapchain
        texture::Dimensions swapchainExtent{}; //!< The extent of images in the current swapchain

        static constexpr size_t MaxSwapchainImageCount{6}; //!< The maximum amount of swapchain textures, this affects the amount of images that can be in the swapchain
        std::array<std::shared_ptr<Texture>, MaxSwapchainImageCount> images; //!< All the swapchain textures in the same order as supplied by the host swapchain

        u64 frameTimestamp{}; //!< The timestamp of the last frame being shown
        perfetto::Track presentationTrack; //!< Perfetto track used for presentation events

        std::thread choreographerThread; //!< A thread for signalling the V-Sync event using AChoreographer
        ALooper *choreographerLooper{}; //!< The looper object associated with the Choreographer thread

        /**
         * @brief The entry point for the the Choreographer thread, the function runs ALooper on the thread
         */
        void ChoreographerThread();

        /**
         * @note 'PresentationEngine::mutex' **must** be locked prior to calling this
         */
        void UpdateSwapchain(texture::Format format, texture::Dimensions extent);

      public:
        std::shared_ptr<kernel::type::KEvent> vsyncEvent; //!< Signalled every time a frame is drawn

        PresentationEngine(const DeviceState &state, GPU &gpu);

        ~PresentationEngine();

        /**
         * @brief Replaces the underlying Android surface with a new one, it handles resetting the swapchain and such
         */
        void UpdateSurface(jobject newSurface);

        /**
         * @brief Queue the supplied texture to be presented to the screen
         * @param presentId A UUID used to tag this frame for presentation timing readouts
         * @note The texture **must** be locked prior to calling this
         */
        void Present(const std::shared_ptr<Texture> &texture, u64 presentId);

        /**
         * @return A transform that the application should render with to elide costly transforms later
         */
        service::hosbinder::NativeWindowTransform GetTransformHint();
    };
}
