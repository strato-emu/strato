// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/trace.h>
#include <kernel/types/KEvent.h>
#include <services/hosbinder/native_window.h>
#include <services/hosbinder/android_types.h>
#include "texture.h"

struct ANativeWindow;

namespace skyline::gpu {
    /**
     * @brief All host presentation is handled by this, it manages the host surface and swapchain alongside dynamically recreating it when required
     */
    class PresentationEngine {
      private:
        const DeviceState &state;
        const GPU &gpu;
        std::mutex mutex; //!< Synchronizes access to the surface objects
        std::condition_variable surfaceCondition; //!< Allows us to efficiently wait for Vulkan surface to be initialized
        jobject surface{}; //!< The Surface object backing the ANativeWindow

        std::optional<vk::raii::SurfaceKHR> vkSurface; //!< The Vulkan Surface object that is backed by ANativeWindow
        std::optional<service::hosbinder::NativeWindowTransform> transformHint; //!< The optimal transform for the application to render as
        std::optional<vk::raii::SwapchainKHR> vkSwapchain; //!< The Vulkan swapchain and the properties associated with it
        struct SwapchainContext {
            u16 imageCount{};
            i32 dequeuedCount{};
            vk::Format imageFormat{};
            vk::Extent2D imageExtent{};
        } swapchain; //!< The properties of the currently created swapchain

        u64 frameTimestamp{}; //!< The timestamp of the last frame being shown
        perfetto::Track presentationTrack; //!< Perfetto track used for presentation events

        void UpdateSwapchain(u16 imageCount, vk::Format imageFormat, vk::Extent2D imageExtent);

      public:
        texture::Dimensions resolution{};
        i32 format{};
        std::shared_ptr<kernel::type::KEvent> vsyncEvent; //!< Signalled every time a frame is drawn
        std::shared_ptr<kernel::type::KEvent> bufferEvent; //!< Signalled every time a buffer is freed

        PresentationEngine(const DeviceState &state, const GPU& gpu);

        ~PresentationEngine();

        /**
         * @brief Replaces the underlying Android surface with a new one, it handles resetting the swapchain and such
         */
        void UpdateSurface(jobject newSurface);

        /**
         * @brief Creates a Texture object from a GuestTexture as a part of the Vulkan swapchain
         */
        std::shared_ptr<Texture> CreatePresentationTexture(const std::shared_ptr<GuestTexture> &texture, u32 slot);

        /**
         * @param async If to return immediately when a texture is not available
         * @param slot The slot the freed texture is in is written into this, it is untouched if there's an error
         */
        service::hosbinder::AndroidStatus GetFreeTexture(bool async, i32& slot);

        /**
         * @brief Send a texture from a slot to the presentation queue to be displayed
         */
        void Present(i32 slot);

        /**
         * @return A transform that the application should render with to elide costly transforms later
         */
        service::hosbinder::NativeWindowTransform GetTransformHint();
    };
}
