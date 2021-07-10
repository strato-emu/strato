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
        std::condition_variable surfaceCondition; //!< Signalled when a valid Vulkan surface is available
        jobject jSurface{}; //!< The Java Surface object backing the ANativeWindow
        ANativeWindow *window{}; //!< The backing Android Native Window for the surface we draw to, we keep this around to access private APIs not exposed via Vulkan
        service::hosbinder::AndroidRect windowCrop{}; //!< A rectangle with the bounds of the current crop performed on the image prior to presentation
        service::hosbinder::NativeWindowScalingMode windowScalingMode{service::hosbinder::NativeWindowScalingMode::ScaleToWindow}; //!< The mode in which the cropped image is scaled up to the surface
        service::hosbinder::NativeWindowTransform windowTransform{}; //!< The transformation performed on the image prior to presentation
        u64 windowLastTimestamp{}; //!< The last timestamp submitted to the window, 0 or CLOCK_MONOTONIC value

        std::optional<vk::raii::SurfaceKHR> vkSurface; //!< The Vulkan Surface object that is backed by ANativeWindow
        vk::SurfaceCapabilitiesKHR vkSurfaceCapabilities; //!< The capabilities of the current Vulkan Surface

        std::optional<vk::raii::SwapchainKHR> vkSwapchain; //!< The Vulkan swapchain and the properties associated with it
        vk::raii::Fence acquireFence; //!< A fence for acquiring an image from the swapchain
        texture::Format swapchainFormat{}; //!< The image format of the textures in the current swapchain
        texture::Dimensions swapchainExtent{}; //!< The extent of images in the current swapchain

        static constexpr size_t MaxSwapchainImageCount{6}; //!< The maximum amount of swapchain textures, this affects the amount of images that can be in the swapchain
        std::array<std::shared_ptr<Texture>, MaxSwapchainImageCount> images; //!< All the swapchain textures in the same order as supplied by the host swapchain

        i64 frameTimestamp{}; //!< The timestamp of the last frame being shown in nanoseconds
        i64 averageFrametimeNs{}; //!< The average time between frames in nanoseconds
        i64 averageFrametimeDeviationNs{}; //!< The average deviation of frametimes in nanoseconds
        perfetto::Track presentationTrack; //!< Perfetto track used for presentation events

        std::thread choreographerThread; //!< A thread for signalling the V-Sync event and measure the refresh cycle duration using AChoreographer
        ALooper *choreographerLooper{};
        i64 lastChoreographerTime{}; //!< The timestamp of the last invocation of Choreographer::doFrame
        i64 refreshCycleDuration{}; //!< The duration of a single refresh cycle for the display in nanoseconds

        /**
         * @url https://developer.android.com/ndk/reference/group/choreographer#achoreographer_postframecallback64
         */
        static void ChoreographerCallback(int64_t frameTimeNanos, PresentationEngine *engine);

        /**
         * @brief The entry point for the the Choreographer thread, sets up the AChoreographer callback then runs ALooper on the thread
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
         * @param timestamp The earliest timestamp (relative to skyline::util::GetTickNs) at which the frame must be presented, it should be 0 when it doesn't matter
         * @param swapInterval The amount of display refreshes that must take place prior to presenting this image
         * @param crop A rectangle with bounds that the image will be cropped to
         * @param scalingMode The mode by which the image must be scaled up to the surface
         * @param transform A transformation that should be performed on the image
         * @param frameId The ID of this frame for correlating it with presentation timing readouts
         * @note The texture **must** be locked prior to calling this
         */
        void Present(const std::shared_ptr<Texture> &texture, u64 timestamp, u64 swapInterval, service::hosbinder::AndroidRect crop, service::hosbinder::NativeWindowScalingMode scalingMode, service::hosbinder::NativeWindowTransform transform, u64 &frameId);

        /**
         * @return A transform that the application should render with to elide costly transforms later
         */
        service::hosbinder::NativeWindowTransform GetTransformHint();
    };
}
